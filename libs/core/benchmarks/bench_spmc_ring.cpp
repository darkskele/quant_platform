#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>

#include "spmc_ring.hpp"
#include "types.hpp"

namespace {

// Single-threaded push-then-try_pop round trip, one consumer, always
// caught up before the next push — never enters the gating wait, so this
// is the per-operation overhead floor (min_cursor scan + atomics +
// construction), same shape as SpscQueue's BM_PushPopInt.
void BM_PushTryPopInt(benchmark::State& state) {
    qp::SpmcRing<int, 1024, 1> ring;
    int                        i = 0;
    for (auto _ : state) {
        ring.push(i);
        auto v = ring.try_pop(0);
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_PushTryPopInt);

// The real production shape: shared_ptr<const MarketEvent>. One allocation
// per push (make_shared); try_pop's copy is a refcount bump, not a
// MarketEvent copy — this is the cost that motivated the design.
void BM_PushTryPopSharedMarketEvent(benchmark::State& state) {
    qp::SpmcRing<std::shared_ptr<const qp::MarketEvent>, 1024, 1> ring;

    qp::MarketEvent seed;
    seed.kind = qp::EventKind::BookDiff;
    seed.bids = {{100.00, 1.0}, {99.50, 2.0}, {99.00, 0.5}};
    seed.asks = {{100.50, 1.5}, {101.00, 0.75}};

    for (auto _ : state) {
        ring.push(std::make_shared<const qp::MarketEvent>(seed));
        auto v = ring.try_pop(0);
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_PushTryPopSharedMarketEvent);

// min_cursor()'s gating scan is O(NumConsumers) per push — this measures
// how that cost actually scales as more engines subscribe. Every consumer
// is popped each iteration (kept caught up), isolating the scan cost from
// any wait/contention cost.
template <std::size_t NumConsumers>
void BM_PushAllConsumersCaughtUp(benchmark::State& state) {
    qp::SpmcRing<int, 1024, NumConsumers> ring;
    int                                   i = 0;
    for (auto _ : state) {
        ring.push(i);
        for (std::size_t c = 0; c < NumConsumers; ++c) {
            auto v = ring.try_pop(c);
            benchmark::DoNotOptimize(v);
        }
        ++i;
    }
}

BENCHMARK(BM_PushAllConsumersCaughtUp<1>);
BENCHMARK(BM_PushAllConsumersCaughtUp<4>);
BENCHMARK(BM_PushAllConsumersCaughtUp<8>);

}  // namespace
