#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "backtest_in_process_transport.hpp"
#include "spmc_queue.hpp"
#include "types.hpp"

namespace {

qp::MarketEvent make_trade(qp::Timestamp ts = 0) {
    qp::TradeEvent ev;
    ev.ts    = ts;
    ev.price = 100.0;
    ev.qty   = 1.0;
    return ev;
}

// kLevels matches a realistic partial-depth update.
qp::MarketEvent make_book_diff(qp::Timestamp ts, int kLevels = 20) {
    qp::BookDiffEvent ev;
    ev.ts       = ts;
    auto levels = std::make_shared<qp::BookLevels>();
    levels->bids.assign(kLevels, qp::PriceLevel{.price = 100.0, .qty = 1.0});
    levels->asks.assign(kLevels, qp::PriceLevel{.price = 101.0, .qty = 1.0});
    ev.levels = std::move(levels);
    return ev;
}

constexpr std::size_t kCapacity = 1024;
using Queue                     = qp::SpmcQueue<qp::MarketEvent, kCapacity, 1, /*UseHeap=*/true>;

// Scaling: N queues merged in timestamp order, N a compile-time template
// parameter.
template <std::size_t N>
void BM_BacktestInProcessTransport_NRings(benchmark::State& state) {
    std::array<Queue, N>       queues;
    std::array<Queue*, N>      queue_ptrs;
    std::array<std::size_t, N> consumers{};
    for (std::size_t i = 0; i < N; ++i) queue_ptrs[i] = &queues[i];

    qp::engine::transport::BacktestInProcessTransport<kCapacity, N> transport(queue_ptrs,
                                                                              consumers);

    qp::Timestamp ts = 0;
    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) queues[i].push(make_trade(ts++));
        for (std::size_t i = 0; i < N; ++i) {
            auto out = transport.next();
            benchmark::DoNotOptimize(out);
        }
    }
}

BENCHMARK(BM_BacktestInProcessTransport_NRings<2>);
BENCHMARK(BM_BacktestInProcessTransport_NRings<4>);
BENCHMARK(BM_BacktestInProcessTransport_NRings<8>);
BENCHMARK(BM_BacktestInProcessTransport_NRings<16>);

// Populated depth, N=2.
void BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff(benchmark::State& state) {
    constexpr std::size_t      N = 2;
    std::array<Queue, N>       queues;
    std::array<Queue*, N>      queue_ptrs;
    std::array<std::size_t, N> consumers{};
    for (std::size_t i = 0; i < N; ++i) queue_ptrs[i] = &queues[i];

    qp::engine::transport::BacktestInProcessTransport<kCapacity, N> transport(queue_ptrs,
                                                                              consumers);

    qp::Timestamp ts = 0;
    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) queues[i].push(make_book_diff(ts++));
        for (std::size_t i = 0; i < N; ++i) {
            auto out = transport.next();
            benchmark::DoNotOptimize(out);
        }
    }
}

BENCHMARK(BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff);

// Real contention: N producer threads, each owning one leg's queue, racing
// the consumer thread's next() calls on a shared transport.
template <std::size_t N>
void BM_BacktestInProcessTransport_NRingsContended(benchmark::State& state) {
    static std::array<Queue, N>  queues;
    static std::array<Queue*, N> queue_ptrs = [] {
        std::array<Queue*, N> ptrs;
        for (std::size_t i = 0; i < N; ++i) ptrs[i] = &queues[i];
        return ptrs;
    }();
    static std::array<std::size_t, N>                                      consumers{};
    static qp::engine::transport::BacktestInProcessTransport<kCapacity, N> transport(queue_ptrs,
                                                                                     consumers);

    bool is_consumer = state.thread_index() == static_cast<int>(N);

    if (is_consumer) {
        transport.flush();
        for (auto _ : state) {
            for (std::size_t n = 0; n < N; ++n) {
                std::optional<qp::engine::transport::EngineInput> out;
                while (!(out = transport.next())) {
                }
                benchmark::DoNotOptimize(out);
            }
        }
    } else {
        auto          leg = static_cast<std::size_t>(state.thread_index());
        qp::Timestamp ts  = 0;
        for (auto _ : state) {
            while (!queues[leg].push(make_trade(ts))) {
            }
            ts += static_cast<qp::Timestamp>(N);  // legs interleave, never collide on ts
        }
    }
}

BENCHMARK(BM_BacktestInProcessTransport_NRingsContended<2>)->Threads(3);
BENCHMARK(BM_BacktestInProcessTransport_NRingsContended<4>)->Threads(5);
BENCHMARK(BM_BacktestInProcessTransport_NRingsContended<8>)->Threads(9);

}  // namespace
