#include "qp/core/spsc_queue.hpp"
#include "qp/core/types.hpp"

#include <benchmark/benchmark.h>

#include <utility>

namespace {

// Single-threaded push-then-pop round trip — no real cross-thread
// contention, so this is the per-operation overhead floor (ring-buffer
// bookkeeping + atomics + object construction), not a measure of
// throughput under actual producer/consumer contention.

void BM_PushPopInt(benchmark::State& state) {
    qp::SpscQueue<int, 1024> q;
    int                      i = 0;
    for (auto _ : state) {
        bool pushed = q.push(i);
        benchmark::DoNotOptimize(pushed);
        auto v = q.pop();
        benchmark::DoNotOptimize(v);
        ++i;
    }
}
BENCHMARK(BM_PushPopInt);

// MarketEvent is the type this queue actually carries in production. A
// fresh copy each iteration deliberately pays vector reallocation — that's
// the real cost of live_ws_source.cpp's current on_message lambda, which
// constructs a new MarketEvent per WS message rather than reusing one (a
// known, not-yet-fixed gap, tracked in that file's own comments).
void BM_PushPopMarketEvent(benchmark::State& state) {
    qp::SpscQueue<qp::MarketEvent, 1024> q;

    qp::MarketEvent seed;
    seed.kind = qp::EventKind::BookDiff;
    seed.bids = {{100.00, 1.0}, {99.50, 2.0}, {99.00, 0.5}};
    seed.asks = {{100.50, 1.5}, {101.00, 0.75}};

    for (auto _ : state) {
        qp::MarketEvent fresh = seed;  // copy: reallocates bids/asks, matches live_ws_source.cpp today
        bool            pushed = q.push(std::move(fresh));
        benchmark::DoNotOptimize(pushed);
        auto v = q.pop();
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_PushPopMarketEvent);

}  // namespace

BENCHMARK_MAIN();
