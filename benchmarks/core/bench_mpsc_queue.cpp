#include <benchmark/benchmark.h>

#include <cstddef>
#include <optional>

#include "mpsc_queue.hpp"

namespace {

// Isolation: push alone, one producer. Periodically (untimed) drains so
// push() never spends timed iterations returning false because the queue
// filled up — same reasoning as SpscQueue's BM_Mpsc_PushInt.
void BM_Mpsc_PushInt(benchmark::State& state) {
    qp::MpscQueue<int, 1024> q;
    int                      i = 0;
    for (auto _ : state) {
        if (!q.push(i)) {
            state.PauseTiming();
            while (q.try_pop()) {
            }
            state.ResumeTiming();
            q.push(i);
        }
        // ClobberMemory() for consistency with SpscQueue/SpmcRing's own
        // isolated push benchmarks — verified on SpscQueue that this class
        // of fix doesn't actually change the measured number (push is
        // genuinely cheaper than pop/try_pop, not an artifact of an elided
        // write), so not re-litigated here independently.
        benchmark::ClobberMemory();
        ++i;
    }
}

BENCHMARK(BM_Mpsc_PushInt);

// Isolation: try_pop alone. Mirror image of BM_Mpsc_PushInt.
void BM_Mpsc_TryPopInt(benchmark::State& state) {
    qp::MpscQueue<int, 1024> q;
    for (int j = 0; j < 1023; ++j) q.push(j);
    for (auto _ : state) {
        auto v = q.try_pop();
        if (!v) {
            state.PauseTiming();
            for (int j = 0; j < 1023; ++j) q.push(j);
            state.ResumeTiming();
            v = q.try_pop();
        }
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Mpsc_TryPopInt);

// Tandem: single-threaded push-then-try_pop round trip — no real
// contention, so this is the per-operation overhead floor (CAS + atomics +
// construction), same shape as SpscQueue's BM_Mpsc_PushPopInt.
void BM_Mpsc_PushPopInt(benchmark::State& state) {
    qp::MpscQueue<int, 1024> q;
    int                      i = 0;
    for (auto _ : state) {
        bool pushed = q.push(i);
        benchmark::DoNotOptimize(pushed);
        auto v = q.try_pop();
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_Mpsc_PushPopInt);

// Contention: NumProducers+1 real OS threads sharing one queue — threads
// 0..NumProducers-1 push (racing the CAS on enqueue_pos_), the last thread
// is the single consumer. `static` shares the queue across the ->Threads()
// group; Google Benchmark barriers every thread before the loop starts and
// before any exits, same pattern as bench_spsc_queue.cpp/bench_spmc_ring.cpp.
//
// Every thread in a ->Threads() group runs the SAME iteration count K per
// repetition — unlike SPSC/SPMC's naturally-1:1 push:pop, that means
// NumProducers threads each pushing once per iteration is K*NumProducers
// pushes against a consumer that only gets K loop trips. First version of
// this benchmark gave the consumer one try_pop() per iteration like
// everyone else and deadlocked: once the queue filled, the consumer
// finished its K iterations and blocked at Google Benchmark's loop-exit
// barrier (which waits for every thread), while the producers' remaining
// iterations blocked forever on a full queue nobody was draining anymore.
// Fix: the consumer pops NumProducers times per iteration, keeping supply
// and demand balanced at every iteration boundary, not just on average.
template <std::size_t NumProducers>
void BM_Mpsc_PushPopContended(benchmark::State& state) {
    static qp::MpscQueue<int, 1024> q;
    bool is_consumer = state.thread_index() == static_cast<int>(NumProducers);

    if (is_consumer) {
        while (q.try_pop()) {
        }
    }

    if (is_consumer) {
        for (auto _ : state) {
            for (std::size_t n = 0; n < NumProducers; ++n) {
                std::optional<int> v;
                while (!(v = q.try_pop())) {
                }
                benchmark::DoNotOptimize(v);
            }
        }
    } else {
        int i = state.thread_index();
        for (auto _ : state) {
            while (!q.push(i)) {
            }
            ++i;
        }
    }
}

BENCHMARK(BM_Mpsc_PushPopContended<1>)->Threads(2);
BENCHMARK(BM_Mpsc_PushPopContended<4>)->Threads(5);
BENCHMARK(BM_Mpsc_PushPopContended<8>)->Threads(9);

}  // namespace
