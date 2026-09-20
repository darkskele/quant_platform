#include <benchmark/benchmark.h>

#include <cstddef>
#include <optional>

#include "work_queue.hpp"

namespace {

// Draining once every ~1023 pushes rather than every iteration, so the
// Pause/Resume cost amortizes to a fraction of a nanosecond per push.
void BM_Work_PushInt(benchmark::State& state) {
    qp::WorkQueue<int, 1024> q;
    int                      i = 0;
    for (auto _ : state) {
        if (!q.push(i)) {
            state.PauseTiming();
            while (q.try_pop()) {
            }
            state.ResumeTiming();
            q.push(i);
        }
        benchmark::ClobberMemory();
        ++i;
    }
}

BENCHMARK(BM_Work_PushInt);

// Isolation: try_pop alone, refilling untimed so it never runs against an
// empty queue. This is the only side that pays a CAS.
void BM_Work_TryPopInt(benchmark::State& state) {
    qp::WorkQueue<int, 1024> q;
    for (int j = 0; j < 1024; ++j) q.push(j);
    for (auto _ : state) {
        auto v = q.try_pop();
        if (!v) {
            state.PauseTiming();
            for (int j = 0; j < 1024; ++j) q.push(j);
            state.ResumeTiming();
            v = q.try_pop();
        }
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Work_TryPopInt);

// Single-threaded round trip, the per-operation floor with no real
// contention on either cursor.
template <bool UseHeap>
void BM_Work_PushPopInt(benchmark::State& state) {
    qp::WorkQueue<int, 1024, UseHeap> q;
    int                               i = 0;
    for (auto _ : state) {
        bool pushed = q.push(i);
        benchmark::DoNotOptimize(pushed);
        auto v = q.try_pop();
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_Work_PushPopInt<false>);
BENCHMARK(BM_Work_PushPopInt<true>);

// One producer against NumConsumers real threads, all sharing one queue, so
// the number is the CAS contention between consumers claiming positions.
template <std::size_t NumConsumers>
void BM_Work_PushTryPopContended(benchmark::State& state) {
    static qp::WorkQueue<int, 1024> q;

    if (state.thread_index() == 0) {
        while (q.try_pop()) {
        }
        int i = 0;
        for (auto _ : state) {
            for (std::size_t n = 0; n < NumConsumers; ++n) {
                while (!q.push(i)) {
                }
                ++i;
            }
        }
    } else {
        for (auto _ : state) {
            std::optional<int> v;
            while (!(v = q.try_pop())) {
            }
            benchmark::DoNotOptimize(v);
        }
    }
}

BENCHMARK(BM_Work_PushTryPopContended<1>)->Threads(2);
BENCHMARK(BM_Work_PushTryPopContended<4>)->Threads(5);
BENCHMARK(BM_Work_PushTryPopContended<8>)->Threads(9);

}  // namespace
