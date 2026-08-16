#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "live_websocket_source.hpp"
#include "support/mock_binance_server.hpp"

using qp::testsupport::collect;
using qp::testsupport::MockHttpServer;
using qp::testsupport::MockWsServer;

namespace {

std::string make_depth_msg(const char* symbol, std::uint64_t u, std::uint64_t pu) {
    return std::string(R"({"stream":"test@depth","data":{"e":"depthUpdate","E":1,"T":1,"s":")") +
           symbol + R"(","U":)" + std::to_string(pu + 1) + R"(,"u":)" + std::to_string(u) +
           R"(,"pu":)" + std::to_string(pu) +
           R"(,"b":[["100.00","1.000"]],"a":[["100.50","1.000"]]}})";
}

std::string make_snapshot_body(std::uint64_t last_update_id) {
    return R"({"lastUpdateId":)" + std::to_string(last_update_id) +
           R"(,"E":1,"T":1,"bids":[],"asks":[]})";
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
    http_server.serve({make_snapshot_body(50)});  // brackets msg 1 (U=1,u=100): 1 <= 50 <= 100

    // Both port strings must outlive `source` — WsEndpoint/RestEndpoint only
    // borrow string_views, read on every reconnect/resync for the source's
    // whole lifetime — declared first so they're destroyed after `source`.
    std::string ws_port  = std::to_string(server.port());
    std::string rest_url = "http://127.0.0.1:" + std::to_string(http_server.port());

    qp::protocol::LiveWebSocketSource source({"BTCUSDT"}, {"127.0.0.1", ws_port, /*use_tls=*/false},
                                             {rest_url});
    // +1 for the BookSnapshot anchor the resync forwards before the diffs.
    auto received = collect(source, 4, std::chrono::seconds(5));

    ASSERT_EQ(received.size(), 4u);
    EXPECT_EQ(received[0].kind, qp::EventKind::BookSnapshot);
    EXPECT_EQ(received[1].seq, 100u);
    EXPECT_EQ(received[2].seq, 105u);
    EXPECT_EQ(received[3].seq, 110u);
    EXPECT_EQ(source.gap_count(), 0u);
    EXPECT_EQ(source.resync_count(), 1u);  // just the initial connect
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

    qp::protocol::LiveWebSocketSource source({"BTCUSDT"}, {"127.0.0.1", ws_port, /*use_tls=*/false},
                                             {rest_url});
    // 2 resyncs (initial + gap), each forwarding its own BookSnapshot anchor
    // before its diff: snapshot, diff(100), snapshot, diff(200).
    auto received = collect(source, 4, std::chrono::seconds(5));

    ASSERT_EQ(received.size(), 4u);
    EXPECT_EQ(received[0].kind, qp::EventKind::BookSnapshot);
    EXPECT_EQ(received[1].seq, 100u);
    EXPECT_EQ(received[2].kind, qp::EventKind::BookSnapshot);
    EXPECT_EQ(received[3].seq, 200u);
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

    qp::protocol::LiveWebSocketSource source({"BTCUSDT"}, {"127.0.0.1", ws_port, /*use_tls=*/false},
                                             {rest_url});
    // Backoff's default initial delay is 1s; give reconnect + two resyncs
    // room. 2 resyncs (one per connection), each forwarding its own
    // BookSnapshot anchor: snapshot, diff(100), snapshot, diff(100).
    auto received = collect(source, 4, std::chrono::seconds(10));

    // Two connections' worth of the same single message = reconnect happened
    // (the second copy looks like a gap from the client's perspective — its
    // prev_seq=0 doesn't match the seq=100 already recorded — which is
    // exactly the mechanism that makes reconnect trigger a fresh resync).
    ASSERT_EQ(received.size(), 4u);
    EXPECT_EQ(received[0].kind, qp::EventKind::BookSnapshot);
    EXPECT_EQ(received[1].seq, 100u);
    EXPECT_EQ(received[2].kind, qp::EventKind::BookSnapshot);
    EXPECT_EQ(received[3].seq, 100u);
    EXPECT_EQ(source.resync_count(), 2u);
}
