#include <gtest/gtest.h>
#include <zstd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <thread>
#include <vector>

#include "../collector.hpp"
#include "support/mock_binance_server.hpp"
#include "support/real_captures.hpp"
#include "support/scratch_dir.hpp"
#include "wire.hpp"

using qp::MarketEvent;
using qp::test::ScratchDir;
using qp::testsupport::MockHttpServer;
using qp::testsupport::MockWsServer;
using qp::testsupport::real_depth_msg;
using qp::testsupport::real_snapshot_body;

namespace {

// Same decode approach as libs/data_source/sink/tests/test_file_recorder_integration.cpp
// — tolerant of an unfinished frame, since that's the real crash-safety
// contract a reader needs to honor. Only the second use of this exact
// helper so far; not worth extracting into shared support yet ("abstract on
// the 3rd implementation").
std::vector<std::byte> decode_whatever_is_readable(const std::filesystem::path& path) {
    std::ifstream     file(path, std::ios::binary);
    std::vector<char> compressed((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

    ZSTD_DStream* dstream = ZSTD_createDStream();
    ZSTD_initDStream(dstream);

    std::vector<std::byte>           out;
    ZSTD_inBuffer                    in{compressed.data(), compressed.size(), 0};
    std::array<std::byte, 64 * 1024> buf;

    while (in.pos < in.size) {
        ZSTD_outBuffer zout{buf.data(), buf.size(), 0};
        std::size_t    ret = ZSTD_decompressStream(dstream, &zout, &in);
        out.insert(out.end(), buf.data(), buf.data() + zout.pos);
        if (ZSTD_isError(ret) || ret == 0) break;
    }
    ZSTD_freeDStream(dstream);
    return out;
}

std::vector<MarketEvent> decode_events(const std::filesystem::path& path) {
    auto                       bytes = decode_whatever_is_readable(path);
    std::span<const std::byte> cursor{bytes};
    std::vector<MarketEvent>   events;
    while (auto ev = qp::wire::read_event(cursor)) events.push_back(std::move(*ev));
    return events;
}

std::filesystem::path only_zst_segment(const std::filesystem::path& symbol_dir) {
    for (const auto& entry : std::filesystem::directory_iterator(symbol_dir)) {
        if (entry.path().extension() == ".zst") return entry.path();
    }
    ADD_FAILURE() << "no .bin.zst segment in " << symbol_dir;
    return {};
}

}  // namespace

// End to end: real WS+HTTP servers, real captured Binance payloads, through
// LiveWebSocketSource's actual resync machinery, into FileRecorder's actual
// zstd-compressed partition — decoded back and checked against what was
// sent. Bounded by --duration's actual mechanism (run_duration), not a test
// hack: this proves that flag for real while also giving the test a clean,
// deterministic stop.
TEST(CollectorIntegration, RecordsRealCapturedEventsEndToEnd) {
    MockWsServer server;
    server.serve({
        real_depth_msg("BTCUSDT", 1, 100, 0),
        real_depth_msg("BTCUSDT", 101, 105, 100),
    });
    MockHttpServer http_server;
    http_server.serve({real_snapshot_body(50)});  // brackets the first message: 1 <= 50 <= 100

    ScratchDir dir;

    std::string ws_port  = std::to_string(server.port());
    std::string rest_url = "http://127.0.0.1:" + std::to_string(http_server.port());

    qp::collector::Config config;
    config.symbols       = {"BTCUSDT"};
    config.data_dir      = dir.path;
    config.ws_endpoint   = {"127.0.0.1", ws_port, /*use_tls=*/false};
    config.rest_endpoint = {rest_url};
    config.run_duration =
        std::chrono::seconds(2);  // the actual --duration feature, exercised directly

    std::atomic<bool> stop_requested{false};  // never set — run_duration alone stops this run
    int               rc = qp::collector::run(config, stop_requested);
    EXPECT_EQ(rc, 0);

    auto symbol_dir = dir.path / "BTCUSDT";
    ASSERT_TRUE(std::filesystem::exists(symbol_dir));

    auto events = decode_events(only_zst_segment(symbol_dir));
    // +1 for the BookSnapshot anchor the resync forwards before the diffs —
    // the real proof that a recording is now self-sufficient, not just
    // diffs with no independent baseline (see docs/decisions.md).
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].kind, qp::EventKind::BookSnapshot);
    EXPECT_EQ(events[0].seq, 50u);         // the snapshot's own last_update_id
    ASSERT_EQ(events[0].bids.size(), 5u);  // real_snapshot_body's actual level count
    EXPECT_EQ(events[0].asks.size(), 5u);
    EXPECT_EQ(events[1].seq, 100u);
    EXPECT_EQ(events[2].seq, 105u);
    ASSERT_EQ(events[1].bids.size(), 16u);  // the real depth-diff capture's actual level count
    EXPECT_EQ(events[1].asks.size(), 8u);
}

TEST(CollectorIntegration, StopRequestedStopsBeforeTheDurationDeadline) {
    MockWsServer server;
    server.serve({real_depth_msg("BTCUSDT", 1, 100, 0)});
    MockHttpServer http_server;
    http_server.serve({real_snapshot_body(50)});

    ScratchDir dir;

    std::string ws_port  = std::to_string(server.port());
    std::string rest_url = "http://127.0.0.1:" + std::to_string(http_server.port());

    qp::collector::Config config;
    config.symbols       = {"BTCUSDT"};
    config.data_dir      = dir.path;
    config.ws_endpoint   = {"127.0.0.1", ws_port, /*use_tls=*/false};
    config.rest_endpoint = {rest_url};
    config.run_duration =
        std::chrono::seconds(60);  // deliberately long — must NOT be why this returns

    std::atomic<bool> stop_requested{false};
    std::thread       stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stop_requested.store(true, std::memory_order_release);
    });

    auto start   = std::chrono::steady_clock::now();
    int  rc      = qp::collector::run(config, stop_requested);
    auto elapsed = std::chrono::steady_clock::now() - start;
    stopper.join();

    EXPECT_EQ(rc, 0);
    EXPECT_LT(elapsed,
              std::chrono::seconds(30));  // stopped by the flag, nowhere near the 60s deadline
}
