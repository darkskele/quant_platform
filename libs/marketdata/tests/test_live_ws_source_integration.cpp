#include "qp/marketdata/live_ws_source.hpp"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

namespace {

// A synchronous acceptor.accept() blocks forever if the expected client
// connection never shows up — a real deadlock risk in test scaffolding that
// asserts a specific request count, not just a style nit. Bounded the same
// way live_ws_source.cpp bounds its own reads: async op + timer, whichever
// finishes first wins. Returns false on timeout (or any accept error).
bool accept_with_timeout(net::io_context& ioc, tcp::acceptor& acceptor, tcp::socket& socket,
                          std::chrono::seconds timeout) {
    ioc.restart();
    net::steady_timer  timer(ioc, timeout);
    beast::error_code  accept_ec = net::error::would_block;  // sentinel: not yet completed
    bool                timed_out = false;

    acceptor.async_accept(socket, [&](beast::error_code ec) {
        accept_ec = ec;
        timer.cancel();  // accept resolved first — don't wait out the rest of the timeout
    });
    timer.async_wait([&](beast::error_code ec) {
        if (!ec) {  // fired for real, wasn't cancelled by the accept completing first
            timed_out = true;
            acceptor.cancel();
        }
    });

    ioc.run();  // returns once both the accept and the timer have resolved, one way or another
    return !timed_out && !accept_ec;
}

// Minimal single-threaded WS server for testing LiveWebSocketSource's real
// connect/handshake/read/reconnect state machine against a local, hermetic
// endpoint — no real Binance dependency, no TLS (that's what
// live_ws_source.cpp's connect_plain() path exists for: WsEndpoint::use_tls
// = false points production's exact transport code at this server instead).
class MockWsServer {
public:
    MockWsServer() : acceptor_(ioc_, tcp::endpoint(tcp::v4(), 0)) {}

    ~MockWsServer() {
        if (thread_.joinable()) thread_.join();
    }

    unsigned short port() const { return acceptor_.local_endpoint().port(); }

    // Serves `messages` to the first connection. If `reconnect` is set,
    // closes after that batch and accepts a second connection to serve
    // `messages` again — proving the client's reconnect-with-backoff
    // actually recovers, not just that it can read once. If the expected
    // connection never arrives (a bug in the client, or a wrong assumption
    // in a test), gives up after kAcceptTimeout rather than hanging the
    // destructor's join() forever.
    //
    // `inter_message_delay`: paced writes, not back-to-back. Without this, a
    // message meant to arrive *after* an earlier one has resynced (testing
    // gap-detection on an already-Streaming symbol) reliably arrives while
    // still Buffering instead — the resync (even local) takes some nonzero
    // time, message delivery over loopback effectively takes none, so
    // "no delay" isn't "arrives immediately after," it's "arrives before
    // resync had a chance to finish." Only needed by tests whose scenario
    // depends on state transitions between messages, not raw delivery.
    void serve(std::vector<std::string> messages, bool reconnect = false,
               std::chrono::milliseconds inter_message_delay = std::chrono::milliseconds(0)) {
        thread_ = std::thread([this, messages = std::move(messages), reconnect, inter_message_delay] {
            if (!serve_one_connection(messages, inter_message_delay)) return;
            if (reconnect && !serve_one_connection(messages, inter_message_delay)) return;
            // Once the script is exhausted, a client racing ahead to a third
            // connect attempt (e.g. during test teardown) should fail fast
            // (connection refused) rather than sit in the accept backlog
            // until its own timeout.
            beast::error_code ec;
            acceptor_.close(ec);
        });
    }

private:
    static constexpr std::chrono::seconds kAcceptTimeout{15};

    bool serve_one_connection(const std::vector<std::string>& messages,
                               std::chrono::milliseconds        inter_message_delay) {
        tcp::socket socket(ioc_);
        if (!accept_with_timeout(ioc_, acceptor_, socket, kAcceptTimeout)) return false;

        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.accept();

        bool first = true;
        for (const auto& msg : messages) {
            if (!first) std::this_thread::sleep_for(inter_message_delay);
            first = false;
            ws.text(true);
            ws.write(net::buffer(msg));
        }

        beast::error_code ec;
        ws.close(websocket::close_code::normal, ec);
        return true;
    }

    net::io_context ioc_;
    tcp::acceptor   acceptor_;
    std::thread     thread_;
};

// Minimal single-threaded HTTP/1.1 server for testing the REST depth-
// snapshot fetch — plain, no TLS, same reasoning as MockWsServer.
class MockHttpServer {
public:
    MockHttpServer() : acceptor_(ioc_, tcp::endpoint(tcp::v4(), 0)) {}

    ~MockHttpServer() {
        if (thread_.joinable()) thread_.join();
    }

    unsigned short port() const { return acceptor_.local_endpoint().port(); }

    // Serves one response body per request, in the order given. If more
    // requests arrive than bodies provided, the last body is reused.
    void serve(std::vector<std::string> bodies) {
        thread_ = std::thread([this, bodies = std::move(bodies)] {
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                if (!serve_one_request(bodies[i])) return;
            }
            beast::error_code ec;
            acceptor_.close(ec);
        });
    }

private:
    static constexpr std::chrono::seconds kAcceptTimeout{15};

    bool serve_one_request(const std::string& body) {
        tcp::socket socket(ioc_);
        if (!accept_with_timeout(ioc_, acceptor_, socket, kAcceptTimeout)) return false;

        beast::flat_buffer              buffer;
        http::request<http::empty_body> req;
        beast::error_code               ec;
        http::read(socket, buffer, req, ec);
        if (ec) return true;  // connection dropped without a request — not fatal to the script

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.keep_alive(false);
        res.body() = body;
        res.prepare_payload();
        http::write(socket, res, ec);

        socket.shutdown(tcp::socket::shutdown_send, ec);
        return true;
    }

    net::io_context ioc_;
    tcp::acceptor   acceptor_;
    std::thread     thread_;
};

std::string make_depth_msg(const char* symbol, std::uint64_t u, std::uint64_t pu) {
    return std::string(R"({"stream":"test@depth","data":{"e":"depthUpdate","E":1,"T":1,"s":")") + symbol +
           R"(","U":)" + std::to_string(pu + 1) + R"(,"u":)" + std::to_string(u) + R"(,"pu":)" +
           std::to_string(pu) + R"(,"b":[["100.00","1.000"]],"a":[["100.50","1.000"]]}})";
}

std::string make_snapshot_body(std::uint64_t last_update_id) {
    return R"({"lastUpdateId":)" + std::to_string(last_update_id) + R"(,"E":1,"T":1,"bids":[],"asks":[]})";
}

// Polls next() until `count` events arrive or `timeout` elapses.
std::vector<qp::MarketEvent> collect(qp::LiveWebSocketSource& source, std::size_t count,
                                      std::chrono::seconds timeout) {
    std::vector<qp::MarketEvent> received;
    auto                         deadline = std::chrono::steady_clock::now() + timeout;
    while (received.size() < count && std::chrono::steady_clock::now() < deadline) {
        if (auto ev = source.next()) {
            received.push_back(std::move(*ev));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    return received;
}

}  // namespace

// Every connection starts a symbol in Buffering (D10) — even this "just
// receive some messages" case exercises a full resync round-trip on the
// first message before anything reaches next().
TEST(LiveWebSocketSourceIntegration, ReceivesMessagesFromLocalServer) {
    MockWsServer server;
    server.serve({
        make_depth_msg("BTCUSDT", 100, 0),
        make_depth_msg("BTCUSDT", 105, 100),
        make_depth_msg("BTCUSDT", 110, 105),
    });
    MockHttpServer http_server;
    http_server.serve({make_snapshot_body(50)});  // brackets msg 1 (U=1,u=100): 1 <= 51 <= 100

    // Both port strings must outlive `source` — WsEndpoint/RestEndpoint only
    // borrow string_views, read on every reconnect/resync for the source's
    // whole lifetime — declared first so they're destroyed after `source`.
    std::string ws_port  = std::to_string(server.port());
    std::string rest_url = "http://127.0.0.1:" + std::to_string(http_server.port());

    qp::LiveWebSocketSource source({"BTCUSDT"}, {"127.0.0.1", ws_port, /*use_tls=*/false}, {rest_url});
    auto                    received = collect(source, 3, std::chrono::seconds(5));

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0].seq, 100u);
    EXPECT_EQ(received[1].seq, 105u);
    EXPECT_EQ(received[2].seq, 110u);
    EXPECT_EQ(source.gap_count(), 0u);
    EXPECT_EQ(source.resync_count(), 1u);       // just the initial connect
    EXPECT_EQ(source.resync_retry_count(), 0u);
}

TEST(LiveWebSocketSourceIntegration, FlagsASequenceGapAndResyncs) {
    MockWsServer server;
    // 500ms between messages: msg 2 needs to arrive *after* msg 1's resync
    // has completed and the symbol is Streaming again, or it just gets
    // swept into the same still-in-progress buffer as msg 1 (still counted
    // as a gap when replayed, but without the second, separate resync this
    // test is actually checking for). Local REST round-trip is normally
    // low tens of ms, so this is generous margin, not a tight race.
    server.serve(
        {
            make_depth_msg("BTCUSDT", 100, 0),
            make_depth_msg("BTCUSDT", 200, 150),  // should continue from 100, claims 150 -> gap
        },
        /*reconnect=*/false, std::chrono::milliseconds(500));
    MockHttpServer http_server;
    http_server.serve({
        make_snapshot_body(50),   // initial-connect resync: brackets msg 1 (U=1,u=100)
        make_snapshot_body(170),  // gap-triggered resync: brackets msg 2 (U=151,u=200)
    });

    std::string ws_port  = std::to_string(server.port());
    std::string rest_url = "http://127.0.0.1:" + std::to_string(http_server.port());

    qp::LiveWebSocketSource source({"BTCUSDT"}, {"127.0.0.1", ws_port, /*use_tls=*/false}, {rest_url});
    auto                    received = collect(source, 2, std::chrono::seconds(5));

    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0].seq, 100u);
    EXPECT_EQ(received[1].seq, 200u);
    EXPECT_EQ(source.gap_count(), 1u);
    EXPECT_EQ(source.resync_count(), 2u);  // initial connect + the gap
}

TEST(LiveWebSocketSourceIntegration, ReconnectsAfterServerDisconnect) {
    MockWsServer server;
    server.serve({make_depth_msg("BTCUSDT", 100, 0)}, /*reconnect=*/true);
    MockHttpServer http_server;
    // Reconnect delivers the identical message again, so both resyncs (one
    // per connection) need to bracket the same (U=1,u=100) pair.
    http_server.serve({make_snapshot_body(50), make_snapshot_body(50)});

    std::string ws_port  = std::to_string(server.port());
    std::string rest_url = "http://127.0.0.1:" + std::to_string(http_server.port());

    qp::LiveWebSocketSource source({"BTCUSDT"}, {"127.0.0.1", ws_port, /*use_tls=*/false}, {rest_url});
    // Backoff's default initial delay is 1s; give reconnect + two resyncs room.
    auto received = collect(source, 2, std::chrono::seconds(10));

    // Two connections' worth of the same single message = reconnect happened
    // (the second copy looks like a gap from the client's perspective — its
    // prev_seq=0 doesn't match the seq=100 already recorded — which is
    // exactly the mechanism that makes reconnect trigger a fresh resync).
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0].seq, 100u);
    EXPECT_EQ(received[1].seq, 100u);
    EXPECT_EQ(source.resync_count(), 2u);
}
