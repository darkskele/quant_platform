#include <benchmark/benchmark.h>

#include "sim_clock.hpp"

namespace {

// sim_clock.hpp and cmake/clock/CMakeLists.txt both document this as
// deliberately unbenchmarked: now()/advance() are a single scalar
// load/store each, inlining away entirely at the Engine loop's call site.
// This exists anyway, for suite-wide coverage consistency — treat it as
// confirming that documented judgment empirically, not as a reason to
// revisit it.
void BM_SimClock_NowAdvance(benchmark::State& state) {
    qp::SimClock  clock;
    qp::Timestamp ts = 0;
    for (auto _ : state) {
        clock.advance(ts);
        auto now = clock.now();
        benchmark::DoNotOptimize(now);
        ++ts;
    }
}

BENCHMARK(BM_SimClock_NowAdvance);

void BM_SimClock_Now(benchmark::State& state) {
    qp::SimClock  clock;
    qp::Timestamp ts = 0;
    for (auto _ : state) {
        auto now = clock.now();
        benchmark::DoNotOptimize(now);
        ++ts;
    }
}

BENCHMARK(BM_SimClock_Now);

void BM_SimClock_Advance(benchmark::State& state) {
    qp::SimClock  clock;
    qp::Timestamp ts = 0;
    for (auto _ : state) {
        clock.advance(ts);
        benchmark::DoNotOptimize(clock);
        ++ts;
    }
}

BENCHMARK(BM_SimClock_Advance);

}  // namespace
