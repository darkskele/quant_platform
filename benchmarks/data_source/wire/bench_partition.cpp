#include <benchmark/benchmark.h>

#include <fstream>
#include <vector>

#include "partition.hpp"
#include "support/scratch_dir.hpp"

using namespace qp::wire;
using qp::Timestamp;
using qp::test::ScratchDir;

namespace {

constexpr Timestamp kTs = 1'700'000'000LL * 1'000'000'000LL;

// Isolation: day_key_for() — a single floor-to-days conversion, the
// currently-open-partition check every write pays.
void BM_Partition_DayKeyFor(benchmark::State& state) {
    for (auto _ : state) benchmark::DoNotOptimize(day_key_for(kTs));
}

BENCHMARK(BM_Partition_DayKeyFor);

// Isolation: needs_rotation() — the pure decision, no I/O.
void BM_Partition_NeedsRotation(benchmark::State& state) {
    DayKey day = day_key_for(kTs);
    for (auto _ : state) benchmark::DoNotOptimize(needs_rotation(day, day));
}

BENCHMARK(BM_Partition_NeedsRotation);

// Isolation: format_day() — only called on an actual rotation (at most
// once per symbol per day, per its own doc comment: "doesn't need to be
// fast, just correct"), benched anyway for suite-wide coverage.
void BM_Partition_FormatDay(benchmark::State& state) {
    DayKey day = day_key_for(kTs);
    for (auto _ : state) benchmark::DoNotOptimize(format_day(day));
}

BENCHMARK(BM_Partition_FormatDay);

// Isolation: segment_path() — builds the on-disk filename for one segment.
// Same rotation-only cadence as format_day(), which this calls internally.
void BM_Partition_SegmentPath(benchmark::State& state) {
    DayKey                day = day_key_for(kTs);
    std::filesystem::path dir = "BTCUSDT";
    for (auto _ : state) benchmark::DoNotOptimize(segment_path(dir, day, 0));
}

BENCHMARK(BM_Partition_SegmentPath);

// Isolation: list_segments() — the one real-filesystem-I/O function here
// (FileReplaySource's own startup cost, enumerating what a symbol/day
// actually has on disk). 5 real pre-created segment files, matching a
// realistic rotation count for one symbol/day.
void BM_Partition_ListSegments(benchmark::State& state) {
    ScratchDir    dir;
    DayKey        day       = day_key_for(kTs);
    constexpr int kSegments = 5;
    for (int seq = 0; seq < kSegments; ++seq) {
        std::ofstream(segment_path(dir.path, day, seq)).put('\0');
    }

    for (auto _ : state) {
        auto segments = list_segments(dir.path, day);
        benchmark::DoNotOptimize(segments.size());
    }
}

BENCHMARK(BM_Partition_ListSegments);

}  // namespace
