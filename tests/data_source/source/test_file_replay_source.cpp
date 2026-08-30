#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "file_replay_source.hpp"
#include "partition.hpp"
#include "support/market_event_builders.hpp"
#include "support/scratch_dir.hpp"
#include "wire.hpp"
#include "zstd_stream.hpp"

using qp::MarketEvent;
using qp::SymbolId;
using qp::data_source::source::FileReplaySource;
using qp::data_source::wire::day_key_for;
using qp::data_source::wire::DayKey;
using qp::test::ScratchDir;

namespace {

constexpr std::int64_t kNanosPerDay = 86400LL * 1'000'000'000LL;
constexpr std::int64_t kBaseTs = 1'700'000'000LL * 1'000'000'000LL;  // arbitrary, real-ish epoch ns
const DayKey           kDay    = day_key_for(kBaseTs);

MarketEvent trade(SymbolId symbol, std::int64_t ts, double price) {
    return qp::test::make_trade(symbol, ts, price);
}

// symbols.manifest, matching FileRecorder's own write_symbols_manifest()
// (now gone along with the rest of the live-collector machinery) — one
// name per line, index == SymbolId.
void write_symbols_manifest(const std::filesystem::path&    dir,
                            const std::vector<std::string>& symbol_names) {
    std::ofstream out(dir / "symbols.manifest");
    for (const auto& name : symbol_names) out << name << '\n';
}

// One on-disk segment for one (symbol, day): every event pushed through one
// ZstdCompressor, one finish()'d frame per file, at the next unused seq for
// that (symbol, day) — same next-free-seq scan FileRecorder::ensure_open
// used, so calling this twice for the same symbol+day produces segment 000
// then 001, matching what a real recorder restart on the same day produces.
void write_segment(const std::filesystem::path& symbol_dir, DayKey day,
                   const std::vector<MarketEvent>& events) {
    std::filesystem::create_directories(symbol_dir);
    int                   seq = 0;
    std::filesystem::path path;
    for (;; ++seq) {
        path = qp::data_source::wire::segment_path(symbol_dir, day, seq);
        if (!std::filesystem::exists(path)) break;
    }

    qp::data_source::wire::ZstdCompressor compressor;
    std::vector<std::byte>                compressed;
    for (const auto& ev : events) {
        std::vector<std::byte> encoded;
        qp::data_source::wire::write_event(encoded, ev);
        compressor.compress(encoded, compressed);
    }
    compressor.finish(compressed);

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(compressed.data()),
              static_cast<std::streamsize>(compressed.size()));
}

// Groups `events` by (symbol, day) and writes each group as its own segment
// — hand-rolled from qp_wire's own primitives, not a real FileRecorder:
// FileRecorder no longer exists as a Sink (removed with the rest of the
// live-collector machinery), so this is now the only way to produce a
// FileReplaySource fixture at all, not an isolation choice.
void record_batch(const std::filesystem::path& dir, const std::vector<std::string>& symbol_names,
                  const std::vector<MarketEvent>& events) {
    write_symbols_manifest(dir, symbol_names);

    std::map<std::pair<SymbolId, DayKey>, std::vector<MarketEvent>> by_symbol_day;
    for (const auto& ev : events)
        by_symbol_day[{qp::header_of(ev).symbol, day_key_for(qp::header_of(ev).ts)}].push_back(ev);

    for (const auto& [key, group] : by_symbol_day) {
        const auto& [symbol, day] = key;
        write_segment(dir / symbol_names.at(symbol), day, group);
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
    EXPECT_EQ(qp::header_of(events[0]).ts, kBaseTs + 100);
    EXPECT_EQ(qp::header_of(events[1]).ts, kBaseTs + 200);
    EXPECT_EQ(qp::header_of(events[2]).ts, kBaseTs + 300);
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
    for (auto& ev : events) got.emplace_back(qp::header_of(ev).ts, qp::header_of(ev).symbol);
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
    EXPECT_EQ(qp::header_of(events[0]).symbol, 0u);
    EXPECT_EQ(qp::header_of(events[1]).symbol, 1u);
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
    EXPECT_EQ(qp::header_of(events[0]).symbol, 1u);
}

TEST(FileReplaySource, ThrowsWhenWantedSymbolIsNotInManifest) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT"}, {trade(0, kBaseTs + 100, 1.0)});

    EXPECT_THROW(FileReplaySource(dir.path, kDay, kDay, std::vector<std::string>{"DOGEUSDT"}),
                 std::runtime_error);
}

TEST(FileReplaySource, ThrowsWhenManifestIsMissing) {
    ScratchDir dir;  // no manifest ever written here
    EXPECT_THROW(FileReplaySource(dir.path, kDay, kDay), std::runtime_error);
}

TEST(FileReplaySource, ReadsAcrossMultipleSegmentsInOrder) {
    ScratchDir dir;
    // Two separate record_batch calls for the same dir+day — exactly what
    // a collector restart on the same day produces: segment 000, then
    // segment 001.
    record_batch(dir.path, {"BTCUSDT"},
                 {trade(0, kBaseTs + 100, 1.0), trade(0, kBaseTs + 200, 2.0)});
    record_batch(dir.path, {"BTCUSDT"}, {trade(0, kBaseTs + 300, 3.0)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(qp::header_of(events[0]).ts, kBaseTs + 100);
    EXPECT_EQ(qp::header_of(events[1]).ts, kBaseTs + 200);
    EXPECT_EQ(qp::header_of(events[2]).ts, kBaseTs + 300);
}

TEST(FileReplaySource, ReadsAcrossMultipleDaysWithinRange) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT"},
                 {trade(0, kBaseTs, 1.0), trade(0, kBaseTs + kNanosPerDay, 2.0)});

    FileReplaySource source(dir.path, kDay, day_key_for(kBaseTs + kNanosPerDay));
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(qp::header_of(events[0]).ts, kBaseTs);
    EXPECT_EQ(qp::header_of(events[1]).ts, kBaseTs + kNanosPerDay);
}

TEST(FileReplaySource, DaysOutsideTheRequestedRangeAreExcluded) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT"},
                 {trade(0, kBaseTs - kNanosPerDay, 0.5), trade(0, kBaseTs, 1.0),
                  trade(0, kBaseTs + kNanosPerDay, 1.5)});

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(qp::header_of(events[0]).ts, kBaseTs);
}

TEST(FileReplaySource, SymbolWithNoRecordedDataYieldsNothingButDoesNotFail) {
    ScratchDir dir;
    record_batch(dir.path, {"BTCUSDT", "ETHUSDT"}, {trade(0, kBaseTs + 100, 1.0)});
    // ETHUSDT is in the manifest but never got an event — no directory, no
    // segments; still a valid, silent symbol, not an error.

    FileReplaySource source(dir.path, kDay, kDay);
    auto             events = drain(source);

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(qp::header_of(events[0]).symbol, 0u);
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
    for (std::size_t i = 0; i < events.size(); ++i)
        EXPECT_EQ(qp::header_of(events[i]).ts, qp::header_of(written[i]).ts);
}
