#include <gtest/gtest.h>
#include <zstd.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <thread>
#include <vector>

#include "file_recorder.hpp"
#include "wire.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::PriceLevel;
using qp::sink::FileRecorder;

namespace {

// Unique-per-test scratch dir under the system temp path, removed on
// destruction — real file I/O needs a real filesystem location, not a mock.
struct ScratchDir {
    std::filesystem::path path;

    ScratchDir() {
        std::random_device rd;
        path = std::filesystem::temp_directory_path() /
               std::filesystem::path("qp_sink_test_" + std::to_string(rd()));
        std::filesystem::create_directories(path);
    }

    ~ScratchDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

MarketEvent book_diff(std::uint32_t symbol, std::int64_t ts, std::uint64_t seq,
                      std::vector<PriceLevel> bids = {{100.0, 1.0}}) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.ts        = ts;
    ev.symbol    = symbol;
    ev.first_seq = seq;
    ev.seq       = seq;
    ev.prev_seq  = seq > 0 ? seq - 1 : 0;
    ev.bids      = std::move(bids);
    return ev;
}

// Decompresses whatever's decodable from `path` — tolerant of an unfinished
// zstd frame (stops cleanly at the last flush checkpoint rather than
// erroring), because that's exactly the situation this exists to verify:
// D12's claim that a flush leaves the file readable up to that point even
// without a clean close. A real reader (FileReplaySource, Phase 1) needs
// this same tolerance for genuine post-crash files, so this isn't testing
// something looser than what production needs to handle anyway.
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

std::filesystem::path only_segment(const std::filesystem::path& symbol_dir,
                                   std::string_view             suffix) {
    for (const auto& entry : std::filesystem::directory_iterator(symbol_dir)) {
        if (entry.path().string().ends_with(std::string(suffix))) return entry.path();
    }
    ADD_FAILURE() << "no file matching " << suffix << " in " << symbol_dir;
    return {};
}

}  // namespace

TEST(FileRecorderIntegration, RecordsAndReadsBackEventsInOrder) {
    ScratchDir dir;
    {
        FileRecorder recorder(dir.path, {"BTCUSDT"});
        recorder.record(book_diff(0, 1'700'000'000'000'000'000, 1));
        recorder.record(book_diff(0, 1'700'000'000'000'000'000, 2, {{101.0, 2.0}, {99.5, 0.5}}));
        recorder.record(book_diff(0, 1'700'000'000'000'000'000, 3));
        // destructor: clean shutdown, drains the queue, finish()es the frame
    }

    auto data_path = only_segment(dir.path / "BTCUSDT", ".bin.zst");
    auto events    = decode_events(data_path);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].seq, 1u);
    EXPECT_EQ(events[1].seq, 2u);
    ASSERT_EQ(events[1].bids.size(), 2u);
    EXPECT_EQ(events[1].bids[0].price, 101.0);
    EXPECT_EQ(events[2].seq, 3u);
}

TEST(FileRecorderIntegration, RotatesToANewSegmentOnADifferentDay) {
    ScratchDir             dir;
    constexpr std::int64_t kDay0 = 1'700'000'000'000'000'000;          // some day
    constexpr std::int64_t kDay1 = kDay0 + 86400LL * 1'000'000'000LL;  // next day
    {
        FileRecorder recorder(dir.path, {"ETHUSDT"});
        recorder.record(book_diff(0, kDay0, 1));
        recorder.record(book_diff(0, kDay1, 2));
    }

    std::size_t bin_files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir.path / "ETHUSDT")) {
        if (entry.path().extension() == ".zst") ++bin_files;
    }
    EXPECT_EQ(bin_files, 2u);
}

TEST(FileRecorderIntegration, LogWritesToTheManifestSidecar) {
    ScratchDir dir;
    {
        FileRecorder recorder(dir.path, {"BTCUSDT"});
        recorder.record(book_diff(0, 1'700'000'000'000'000'000, 1));  // opens the partition
        recorder.log(0, "resync_retry_count=3 (delta +2)");
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));  // let the writer thread catch up
    }

    auto          manifest_path = only_segment(dir.path / "BTCUSDT", ".manifest");
    std::ifstream manifest(manifest_path);
    std::string   contents((std::istreambuf_iterator<char>(manifest)),
                           std::istreambuf_iterator<char>());

    EXPECT_NE(contents.find("BTCUSDT"), std::string::npos);
    EXPECT_NE(contents.find("resync_retry_count=3"), std::string::npos);
}

// The actual crash-safety proof (D12): a periodic flush leaves the file
// readable up to that point *without* a clean shutdown ever happening. This
// reads the file while the FileRecorder is still alive and its zstd frame
// is still open — not after finish() closes it.
TEST(FileRecorderIntegration, FlushLeavesAReadablePrefixWithoutACleanShutdown) {
    ScratchDir   dir;
    FileRecorder recorder(dir.path, {"BTCUSDT"}, std::chrono::milliseconds(30));

    recorder.record(book_diff(0, 1'700'000'000'000'000'000, 1));
    recorder.record(book_diff(0, 1'700'000'000'000'000'000, 2));

    // Wait past at least one flush cycle — no clean shutdown, no finish().
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    auto data_path = only_segment(dir.path / "BTCUSDT", ".bin.zst");
    auto events    = decode_events(data_path);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].seq, 1u);
    EXPECT_EQ(events[1].seq, 2u);
    // `recorder` goes out of scope normally after this — not part of what's
    // being verified, just ordinary test cleanup.
}
