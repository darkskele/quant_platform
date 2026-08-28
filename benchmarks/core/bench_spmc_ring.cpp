#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>
#include <optional>

#include "spmc_ring.hpp"
#include "types.hpp"

namespace {

// Isolation: push alone. Unlike SpscQueue::push, SpmcRing::push() has no
// "full" return value — it blocks (spin/yield/backoff) until the slowest
// consumer catches up — so there's no failed-push signal to drain on.
// Instead, drain proactively every 1000 pushes (comfortably under the 1024
// capacity) so push() never actually enters wait_for_slot's slow path
// during a timed iteration.
void BM_Spmc_PushInt(benchmark::State& state) {
    qp::SpmcRing<int, 1024, 1> ring;
    int                        i = 0;
    for (auto _ : state) {
        if (i % 1000 == 0 && i != 0) {
            state.PauseTiming();
            while (ring.try_pop(0)) {
            }
            state.ResumeTiming();
        }
        ring.push(i);
        ++i;
    }
}

BENCHMARK(BM_Spmc_PushInt);

// Isolation: try_pop alone. Mirror image of BM_Spmc_PushInt: periodically
// (untimed) refills so try_pop() never spends timed iterations against an
// empty ring.
void BM_Spmc_TryPopInt(benchmark::State& state) {
    qp::SpmcRing<int, 1024, 1> ring;
    for (int j = 0; j < 1000; ++j) ring.push(j);
    for (auto _ : state) {
        auto v = ring.try_pop(0);
        if (!v) {
            state.PauseTiming();
            for (int j = 0; j < 1000; ++j) ring.push(j);
            state.ResumeTiming();
            v = ring.try_pop(0);
        }
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Spmc_TryPopInt);

// Tandem: single-threaded push-then-try_pop round trip, one consumer,
// always caught up before the next push — never enters the gating wait, so
// this is the per-operation overhead floor (min_cursor scan + atomics +
// construction), same shape as SpscQueue's BM_Spsc_PushPopInt.
void BM_Spmc_PushTryPopInt(benchmark::State& state) {
    qp::SpmcRing<int, 1024, 1> ring;
    int                        i = 0;
    for (auto _ : state) {
        ring.push(i);
        auto v = ring.try_pop(0);
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_Spmc_PushTryPopInt);

// The real production shape: shared_ptr<const MarketEvent>. One allocation
// per push (make_shared); try_pop's copy is a refcount bump, not a
// MarketEvent copy — this is the cost that motivated the design.
void BM_Spmc_PushTryPopSharedMarketEvent(benchmark::State& state) {
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

BENCHMARK(BM_Spmc_PushTryPopSharedMarketEvent);

// min_cursor()'s gating scan is O(NumConsumers) per push — this measures
// how that cost actually scales as more engines subscribe. Every consumer
// is popped each iteration (kept caught up), isolating the scan cost from
// any wait/contention cost.
template <std::size_t NumConsumers>
void BM_Spmc_PushAllConsumersCaughtUp(benchmark::State& state) {
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

BENCHMARK(BM_Spmc_PushAllConsumersCaughtUp<1>);
BENCHMARK(BM_Spmc_PushAllConsumersCaughtUp<4>);
BENCHMARK(BM_Spmc_PushAllConsumersCaughtUp<8>);

// Contention: one shared ring, NumConsumers+1 real OS threads — thread 0 is
// the producer, threads 1..NumConsumers are independent real consumers,
// each spinning against the producer's pace on its own cursor. Same
// pattern as bench_spsc_queue.cpp's BM_Spsc_PushPopContended (`static`
// shares the ring across the ->Threads() group; Google Benchmark barriers
// every thread before the loop starts and before any exits), extended to
// SpmcRing's actual multi-consumer shape — this is the real "does adding
// more subscribed engines make the producer's push()/wait_for_slot cost
// worse under genuine contention" number that BM_Spmc_PushAllConsumersCaughtUp
// above can't give you (it's one thread doing everything, never contended).
template <std::size_t NumConsumers>
void BM_Spmc_PushTryPopContended(benchmark::State& state) {
    static qp::SpmcRing<int, 1024, NumConsumers> ring;

    if (state.thread_index() != 0) {
        auto consumer = static_cast<std::size_t>(state.thread_index() - 1);
        while (ring.try_pop(consumer)) {
        }
    }

    if (state.thread_index() == 0) {
        int i = 0;
        for (auto _ : state) {
            ring.push(i);
            ++i;
        }
    } else {
        auto consumer = static_cast<std::size_t>(state.thread_index() - 1);
        for (auto _ : state) {
            std::optional<int> v;
            while (!(v = ring.try_pop(consumer))) {
            }
            benchmark::DoNotOptimize(v);
        }
    }
}

BENCHMARK(BM_Spmc_PushTryPopContended<1>)->Threads(2);
BENCHMARK(BM_Spmc_PushTryPopContended<4>)->Threads(5);
BENCHMARK(BM_Spmc_PushTryPopContended<8>)->Threads(9);

}  // namespace
