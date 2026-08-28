#include <benchmark/benchmark.h>

#include <cstdint>

#include "resync_policy.hpp"

using qp::source::FuturesAlignment;
using qp::source::SpotAlignment;

namespace {

// Isolation: FuturesAlignment's two comparisons — brackets() is the
// one-time resync-boundary check, continues() the steady-state per-diff
// check (resync_policy.hpp). Both are 1-2 integer comparisons; benched for
// suite-wide coverage, not because either was suspected to be costly.
void BM_FuturesAlignment_Brackets(benchmark::State& state) {
    std::uint64_t U = 100, last_update_id = 105, u = 110;
    for (auto _ : state) benchmark::DoNotOptimize(FuturesAlignment::brackets(U, last_update_id, u));
}

BENCHMARK(BM_FuturesAlignment_Brackets);

void BM_FuturesAlignment_Continues(benchmark::State& state) {
    std::uint64_t first_seq = 0, prev_seq = 100, last_seq = 100;
    for (auto _ : state)
        benchmark::DoNotOptimize(FuturesAlignment::continues(first_seq, prev_seq, last_seq));
}

BENCHMARK(BM_FuturesAlignment_Continues);

// Isolation: SpotAlignment's equivalents — same shape, one more addition
// each (resync_policy.hpp's documented `last_update_id+1`/`last_seq+1`
// offset vs futures' none).
void BM_SpotAlignment_Brackets(benchmark::State& state) {
    std::uint64_t U = 100, last_update_id = 104, u = 110;
    for (auto _ : state) benchmark::DoNotOptimize(SpotAlignment::brackets(U, last_update_id, u));
}

BENCHMARK(BM_SpotAlignment_Brackets);

void BM_SpotAlignment_Continues(benchmark::State& state) {
    std::uint64_t first_seq = 101, prev_seq = 0, last_seq = 100;
    for (auto _ : state)
        benchmark::DoNotOptimize(SpotAlignment::continues(first_seq, prev_seq, last_seq));
}

BENCHMARK(BM_SpotAlignment_Continues);

}  // namespace
