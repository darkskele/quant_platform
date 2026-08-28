#include <benchmark/benchmark.h>

#include "backoff.hpp"

using qp::source::ExponentialBackoff;

namespace {

// Isolation: next() — a compare/multiply/store, called once per reconnect
// attempt (not a hot per-message path), but simple enough it's worth
// confirming the cost floor is negligible rather than assuming it.
void BM_ExponentialBackoff_Next(benchmark::State& state) {
    ExponentialBackoff backoff;
    for (auto _ : state) benchmark::DoNotOptimize(backoff.next());
}

BENCHMARK(BM_ExponentialBackoff_Next);

// Isolation: reset() — called once per successful (re)connection.
void BM_ExponentialBackoff_Reset(benchmark::State& state) {
    ExponentialBackoff backoff;
    backoff.next();
    for (auto _ : state) {
        backoff.reset();
        benchmark::DoNotOptimize(backoff);
    }
}

BENCHMARK(BM_ExponentialBackoff_Reset);

}  // namespace
