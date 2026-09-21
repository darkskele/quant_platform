#include <benchmark/benchmark.h>

#include <cstddef>
#include <memory>
#include <optional>

#include "spmc_queue.hpp"
#include "types.hpp"

namespace {

void BM_Spmc_PushInt(benchmark::State& state) {
    qp::SpmcQueue<int, 1024, 1> queue;
    int                         i = 0;
    for (auto _ : state) {
        if (i % 1000 == 0 && i != 0) {
            state.PauseTiming();
            while (queue.try_pop(0)) {
            }
            state.ResumeTiming();
        }
        bool ok = queue.push(i);
        benchmark::DoNotOptimize(ok);
        ++i;
    }
}

BENCHMARK(BM_Spmc_PushInt);

void BM_Spmc_TryPopInt(benchmark::State& state) {
    qp::SpmcQueue<int, 1024, 1> queue;
    for (int j = 0; j < 1000; ++j) queue.push(j);
    for (auto _ : state) {
        auto v = queue.try_pop(0);
        if (!v) {
            state.PauseTiming();
            for (int j = 0; j < 1000; ++j) queue.push(j);
            state.ResumeTiming();
            v = queue.try_pop(0);
        }
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Spmc_TryPopInt);

template <bool UseHeap>
void BM_Spmc_PushTryPopInt(benchmark::State& state) {
    qp::SpmcQueue<int, 1024, 1, UseHeap> queue;
    int                                  i = 0;
    for (auto _ : state) {
        queue.push(i);
        auto v = queue.try_pop(0);
        benchmark::DoNotOptimize(v);
        ++i;
    }
}

BENCHMARK(BM_Spmc_PushTryPopInt<false>);
BENCHMARK(BM_Spmc_PushTryPopInt<true>);

template <bool UseHeap>
void BM_Spmc_PushTryPopMarketEvent(benchmark::State& state) {
    qp::SpmcQueue<qp::MarketEvent, 1024, 1, UseHeap> queue;

    qp::BookDepthEvent seed;
    seed.bands = std::make_shared<const qp::BookDepthBands>();

    for (auto _ : state) {
        queue.push(qp::MarketEvent{.base = {.kind = qp::EventKind::BookDepth}, .payload = seed});
        auto v = queue.try_pop(0);
        benchmark::DoNotOptimize(v);
    }
}

BENCHMARK(BM_Spmc_PushTryPopMarketEvent<false>);
BENCHMARK(BM_Spmc_PushTryPopMarketEvent<true>);

template <std::size_t NumConsumers>
void BM_Spmc_PushAllConsumersCaughtUp(benchmark::State& state) {
    qp::SpmcQueue<int, 1024, NumConsumers> queue;
    int                                    i = 0;
    for (auto _ : state) {
        queue.push(i);
        for (std::size_t c = 0; c < NumConsumers; ++c) {
            auto v = queue.try_pop(c);
            benchmark::DoNotOptimize(v);
        }
        ++i;
    }
}

BENCHMARK(BM_Spmc_PushAllConsumersCaughtUp<1>);
BENCHMARK(BM_Spmc_PushAllConsumersCaughtUp<4>);
BENCHMARK(BM_Spmc_PushAllConsumersCaughtUp<8>);

template <std::size_t NumConsumers>
void BM_Spmc_PushTryPopContended(benchmark::State& state) {
    static qp::SpmcQueue<int, 1024, NumConsumers> queue;

    if (state.thread_index() != 0) {
        auto consumer = static_cast<std::size_t>(state.thread_index() - 1);
        while (queue.try_pop(consumer)) {
        }
    }

    if (state.thread_index() == 0) {
        int i = 0;
        for (auto _ : state) {
            while (!queue.push(i)) {
            }
            ++i;
        }
    } else {
        auto consumer = static_cast<std::size_t>(state.thread_index() - 1);
        for (auto _ : state) {
            std::optional<int> v;
            while (!(v = queue.try_pop(consumer))) {
            }
            benchmark::DoNotOptimize(v);
        }
    }
}

BENCHMARK(BM_Spmc_PushTryPopContended<1>)->Threads(2);
BENCHMARK(BM_Spmc_PushTryPopContended<4>)->Threads(5);
BENCHMARK(BM_Spmc_PushTryPopContended<8>)->Threads(9);

}  // namespace
