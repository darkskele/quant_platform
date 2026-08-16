#pragma once

// Minimal local WS + HTTP servers for testing LiveWebSocketSource's (and,
// via it, FileRecorder's/the collector's) real connect/handshake/read/
// resync state machine against a hermetic endpoint — no real Binance
// dependency, no TLS (WsEndpoint::use_tls = false points production's exact
// transport code at these instead). Originally written for
// test_live_websocket_source_integration.cpp; extracted here once
// apps/collector's integration test needed the exact same infrastructure a
// second time — not duplicated a third time.

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "live_websocket_source.hpp"

namespace qp::testsupport {

namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;

// A synchronous acceptor.accept() blocks forever if the expected client
// connection never shows up — a real deadlock risk in test scaffolding that
// asserts a specific request count, not just a style nit. Bounded the same
// way live_websocket_source.cpp bounds its own reads: async op + timer, whichever
// finishes first wins. Returns false on timeout (or any accept error).
inline bool accept_with_timeout(net::io_context& ioc, tcp::acceptor& acceptor, tcp::socket& socket,
                                std::chrono::seconds timeout) {
    ioc.restart();
    net::steady_timer timer(ioc, timeout);
    beast::error_code accept_ec = net::error::would_block;  // sentinel: not yet completed
    bool              timed_out = false;

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

class MockWsServer {
   public:
    MockWsServer() : acceptor_(ioc_, tcp::endpoint(tcp::v4(), 0)) {}

    ~MockWsServer() {
        if (thread_.joinable()) thread_.join();
    }

    unsigned short port() const { return acceptor_.local_endpoint().port(); }

    // Serves `messages` to the first connection. If `reconnect` is set,
    // closes after that batch and accepts a second connection to serve
    // `messages` again. If the expected connection never arrives, gives up
    // after kAcceptTimeout rather than hanging the destructor's join()
    // forever.
    //
    // `inter_message_delay`: paced writes, not back-to-back — needed by any
    // scenario whose outcome depends on state transitions *between*
    // messages (e.g. a resync completing before the next message arrives),
    // since loopback delivery is otherwise effectively instantaneous.
    void serve(std::vector<std::string> messages, bool reconnect = false,
               std::chrono::milliseconds inter_message_delay = std::chrono::milliseconds(0)) {
        thread_ =
            std::thread([this, messages = std::move(messages), reconnect, inter_message_delay] {
                if (!serve_one_connection(messages, inter_message_delay)) return;
                if (reconnect && !serve_one_connection(messages, inter_message_delay)) return;
                beast::error_code ec;
                acceptor_.close(ec);
            });
    }

   private:
    static constexpr std::chrono::seconds kAcceptTimeout{15};

    bool serve_one_connection(const std::vector<std::string>& messages,
                              std::chrono::milliseconds       inter_message_delay) {
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

// Polls next() until `count` events arrive or `timeout` elapses.
inline std::vector<qp::MarketEvent> collect(qp::protocol::LiveWebSocketSource& source,
                                            std::size_t count, std::chrono::seconds timeout) {
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

}  // namespace qp::testsupport
