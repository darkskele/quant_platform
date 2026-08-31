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
        // push() returns void, so there's no return value to anchor a
        // DoNotOptimize on the way BM_Spsc_PushInt does — ClobberMemory()
        // here instead. Same shape of gap as SpscQueue shows (isolated push
        // measures well under isolated try_pop, ~1-2ns vs ~7-9ns) —
        // verified on SpscQueue that this isn't an elided-write artifact
        // (neither ClobberMemory nor DoNotOptimize-on-the-real-return-value
        // changed that number), so treating the same pattern here as
        // genuine rather than re-litigating it structure-by-structure.
        benchmark::ClobberMemory();
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
// construction), same shape as SpscQueue's BM_Spsc_PushPopInt. Templated
// on UseHeap, both instantiated below, to actually measure the "costs the
// same either way" claim in SpmcRing's own UseHeap doc comment instead of
// just asserting it — one extra pointer dereference to reach the backing
// array should be lost in the noise next to atomics + construction, but
// "should be" isn't "measured".
template <bool UseHeap>
void BM_Spmc_PushTryPopInt(benchmark::State& state) {
    qp::SpmcRing<int, 1024, 1, UseHeap> ring;
    int                                 i = 0;
    for (auto _ : state) {
        ring.push(i);
        auto v = ring.try_pop(0);
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_Spmc_PushTryPopInt<false>);
BENCHMARK(BM_Spmc_PushTryPopInt<true>);

// The real production shape (FanoutSink's Ring): MarketEvent directly, no
// shared_ptr — push() move-constructs into the slot (no allocation for
// this seed, a BookDiffEvent, since its levels already live behind their
// own internal shared_ptr<const BookLevels>, core/types.hpp); try_pop's
// copy is a refcount bump on that shared_ptr, not a deep copy of the price
// levels. Templated on UseHeap for the same reason as BM_Spmc_PushTryPopInt
// above, but at MarketEvent's actual width (unlike int, where a heap vs.
// inline difference — if any — would be easiest to see): UseHeap=true is
// what FanoutSink's own ring actually uses (Capacity * sizeof(MarketEvent)
// not worth living inline — see fanout_sink.hpp), UseHeap=false is the
// comparison point.
template <bool UseHeap>
void BM_Spmc_PushTryPopMarketEvent(benchmark::State& state) {
    qp::SpmcRing<qp::MarketEvent, 1024, 1, UseHeap> ring;

    qp::BookDiffEvent seed;
    seed.levels = std::make_shared<const qp::BookLevels>(qp::BookLevels{
        {{100.00, 1.0}, {99.50, 2.0}, {99.00, 0.5}},
        {{100.50, 1.5}, {101.00, 0.75}},
    });

    for (auto _ : state) {
        ring.push(qp::MarketEvent{seed});
        auto v = ring.try_pop(0);
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Spmc_PushTryPopMarketEvent<false>);
BENCHMARK(BM_Spmc_PushTryPopMarketEvent<true>);

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
