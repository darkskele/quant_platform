#include <benchmark/benchmark.h>

#include <cstddef>
#include <optional>

#include "mpsc_queue.hpp"

namespace {

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
