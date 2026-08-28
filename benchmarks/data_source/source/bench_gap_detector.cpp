#include <benchmark/benchmark.h>

#include "gap_detector.hpp"
#include "resync_policy.hpp"

using namespace qp;
using namespace qp::source;

namespace {

// Isolated steady-state cost of check_and_record itself — same symbol,
// always-continuous sequence, so this measures exactly the direct-array-
// index + [[likely]] path, not parse/queue overhead mixed in from the rest
// of GenericLiveWebSocketSource::on_message (no benchmark of that combined
// path exists — see benchmarks/README.md's "What's deliberately not here
// yet" section).
void BM_GapDetector_SteadyState(benchmark::State& state) {
    SequenceGapDetector gaps;
    // seed — first event, establishes the tracked baseline (first_seq unused
    // by FuturesAlignment::continues, 0 is just a filler value here).
    gaps.check_and_record<FuturesAlignment>(1, 0, 0, 100);
    std::uint64_t seq = 100;
    for (auto _ : state) {
        auto gap = gaps.check_and_record<FuturesAlignment>(1, 0, seq, seq + 5);
        benchmark::DoNotOptimize(gap);
        seq += 5;
    }
}

BENCHMARK(BM_GapDetector_SteadyState);

}  // namespace
