#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "file_recorder.hpp"
#include "file_replay_source.hpp"
#include "partition.hpp"
#include "support/market_event_builders.hpp"
#include "support/scratch_dir.hpp"

using qp::MarketEvent;
using qp::SymbolId;
using qp::sink::FileRecorder;
using qp::source::FileReplaySource;
using qp::test::ScratchDir;
using qp::wire::day_key_for;
using qp::wire::DayKey;

namespace {

constexpr std::int64_t kNanosPerDay = 86400LL * 1'000'000'000LL;
constexpr std::int64_t kBaseTs = 1'700'000'000LL * 1'000'000'000LL;  // arbitrary, real-ish epoch ns
const DayKey           kDay    = day_key_for(kBaseTs);

MarketEvent trade(SymbolId symbol, std::int64_t ts, double price) {
    return qp::test::make_trade(symbol, ts, price);
}

// Drives a real FileRecorder to produce the fixture — not a hand-rolled
// reimplementation of its write orchestration (segment naming, when to
// rotate, when to close a frame): using the actual object is what proves
// FileReplaySource agrees with what FileRecorder really does, the same
// reasoning behind qp_parity_tests (tests/test_recorder_replay_parity.cpp).
// record() is async, so the recorder must go out of scope here (destructor:
// drains the queue, flushes, cleanly closes every partition) before the
// caller reads it back. A second call with the same dir+symbols starts a
// fresh FileRecorder instance, which always opens a new segment — exactly
// what a real collector restart on the same day produces. It also (re)writes
// data_dir/symbols.manifest, which FileReplaySource now requires.
//
// record() is non-blocking and silently drops on a full queue by design
// (a live collector can't let a slow disk back up the socket read) — real
// behavior a hand-rolled fixture writer would never exhibit. A tight loop
// of thousands of record() calls can out-run the writer thread in a way a
// live feed's actual arrival rate never would, so retry a dropped event
// rather than assume every call landed.
void record_batch(const std::filesystem::path& dir, const std::vector<std::string>& symbol_names,
                  const std::vector<MarketEvent>& events) {
    FileRecorder recorder(dir, symbol_names);
    for (const auto& ev : events) {
        for (;;) {
            std::size_t dropped_before = recorder.dropped_count();
            recorder.record(ev);
            if (recorder.dropped_count() == dropped_before) break;  // landed
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

std::vector<MarketEvent> drain(FileReplaySource& source) {
    std::vector<MarketEvent> events;
    while (auto ev = source.next()) events.push_back(std::move(*ev));
    return events;
}

}  // namespace

TEST(FileReplaySource, ReplaysSingleSymbolSingleSegmentInOrder) {
    ScratchDir dir;
    record_batch(
        dir.path, {"BTCUSDT"},
        {trade(0, kBaseTs + 100, 1.0), trade(0, kBaseTs + 200, 2.0), trade(0, kBaseTs + 300, 3.0)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].ts, kBaseTs + 100);
    EXPECT_EQ(events[1].ts, kBaseTs + 200);
    EXPECT_EQ(events[2].ts, kBaseTs + 300);
}

TEST(FileReplaySource, MergesMultipleSymbolsByTimestamp) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT", "ETHUSDT"},
                 {trade(0, kBaseTs + 100, 1.0), trade(1, kBaseTs + 200, 2.0),
                  trade(1, kBaseTs + 300, 3.0), trade(0, kBaseTs + 400, 4.0)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 4u);
    std::vector<std::pair<std::int64_t, SymbolId>> got;
    for (auto& ev : events) got.emplace_back(ev.ts, ev.symbol);
    EXPECT_EQ(got,
              (std::vector<std::pair<std::int64_t, SymbolId>>{
                  {kBaseTs + 100, 0}, {kBaseTs + 200, 1}, {kBaseTs + 300, 1}, {kBaseTs + 400, 0}}));
}

TEST(FileReplaySource, BreaksTiesBySymbolIdWhenTimestampsMatch) {
    ScratchDir dir;
    // Recorded ETHUSDT before BTCUSDT to prove the tie-break is by
    // SymbolId, not recording order.
    record_batch(dir.path, {"BTCUSDT", "ETHUSDT"},
                 {trade(1, kBaseTs + 100, 2.0), trade(0, kBaseTs + 100, 1.0)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].symbol, 0u);
    EXPECT_EQ(events[1].symbol, 1u);
}

TEST(FileReplaySource, WantedFilterPreservesManifestSymbolIdNotRenumbered) {
    // ETHUSDT is index 1 in the manifest (BTCUSDT recorded first, even
    // though it produces no events) — filtering the replay down to just
    // ETHUSDT must not renumber it to 0.
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT", "ETHUSDT"}, {trade(1, kBaseTs + 100, 2.0)});

    FileReplaySource source(dir.path, kDay, kDay, std::vector<std::string>{"ETHUSDT"});
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].symbol, 1u);
}

TEST(FileReplaySource, ThrowsWhenWantedSymbolIsNotInManifest) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT"}, {trade(0, kBaseTs + 100, 1.0)});

    EXPECT_THROW(FileReplaySource(dir.path, kDay, kDay, std::vector<std::string>{"DOGEUSDT"}),
                 std::runtime_error);
}

TEST(FileReplaySource, ThrowsWhenManifestIsMissing) {
    ScratchDir dir;  // no FileRecorder ever ran here — no symbols.manifest
    EXPECT_THROW(FileReplaySource(dir.path, kDay, kDay), std::runtime_error);
}

TEST(FileReplaySource, ReadsAcrossMultipleSegmentsInOrder) {
    ScratchDir dir;
    // Two separate FileRecorder instances pointed at the same dir+day —
    // exactly what a collector restart on the same day produces: segment
    // 000, then segment 001.
    record_batch(dir.path, {"BTCUSDT"},
                 {trade(0, kBaseTs + 100, 1.0), trade(0, kBaseTs + 200, 2.0)});
    record_batch(dir.path, {"BTCUSDT"}, {trade(0, kBaseTs + 300, 3.0)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].ts, kBaseTs + 100);
    EXPECT_EQ(events[1].ts, kBaseTs + 200);
    EXPECT_EQ(events[2].ts, kBaseTs + 300);
}

TEST(FileReplaySource, ReadsAcrossMultipleDaysWithinRange) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT"},
                 {trade(0, kBaseTs, 1.0), trade(0, kBaseTs + kNanosPerDay, 2.0)});

    FileReplaySource source(dir.path, kDay, day_key_for(kBaseTs + kNanosPerDay));
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].ts, kBaseTs);
    EXPECT_EQ(events[1].ts, kBaseTs + kNanosPerDay);
}

TEST(FileReplaySource, DaysOutsideTheRequestedRangeAreExcluded) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT"},
                 {trade(0, kBaseTs - kNanosPerDay, 0.5), trade(0, kBaseTs, 1.0),
                  trade(0, kBaseTs + kNanosPerDay, 1.5)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].ts, kBaseTs);
}

TEST(FileReplaySource, SymbolWithNoRecordedDataYieldsNothingButDoesNotFail) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT", "ETHUSDT"}, {trade(0, kBaseTs + 100, 1.0)});
    // ETHUSDT is in the manifest but never got an event — no directory, no
    // segments; still a valid, silent symbol, not an error.

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].symbol, 0u);
}

TEST(FileReplaySource, HandlesManyEventsSpanningMultipleInternalReadChunks) {
    // Large enough to force fill_more() to loop across several reads/
    // decompress calls rather than being satisfied by the first chunk —
    // exercises the decoded_buf compaction/refill path, not just the
    // single-shot case every other test above covers.
    ScratchDir dir;

    std::vector<MarketEvent> written;
    written.reserve(20'000);
    for (int i = 0; i < 20'000; ++i)
        written.push_back(trade(0, kBaseTs + i, static_cast<double>(i)));
    record_batch(dir.path, {"BTCUSDT"}, written);

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), written.size());
    for (std::size_t i = 0; i < events.size(); ++i) EXPECT_EQ(events[i].ts, written[i].ts);
}
