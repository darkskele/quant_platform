#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "backtest_in_process_transport.hpp"
#include "control_channel.hpp"
#include "spmc_ring.hpp"
#include "types.hpp"

namespace {

qp::MarketEvent make_trade(qp::Timestamp ts = 0) {
    qp::TradeEvent ev;
    ev.ts    = ts;
    ev.price = 100.0;
    ev.qty   = 1.0;
    return ev;
}

// kLevels matches a realistic partial-depth update (Binance's 20-level
// partial book stream) — Trade above never carries book levels at all, so
// it can't exercise next()'s move-vs-copy on a populated BookLevels
// regardless of scale.
qp::MarketEvent make_book_diff(qp::Timestamp ts, int kLevels = 20) {
    qp::BookDiffEvent ev;
    ev.ts       = ts;
    auto levels = std::make_shared<qp::BookLevels>();
    levels->bids.assign(kLevels, qp::PriceLevel{.price = 100.0, .qty = 1.0});
    levels->asks.assign(kLevels, qp::PriceLevel{.price = 101.0, .qty = 1.0});
    ev.levels = std::move(levels);
    return ev;
}

using Ring = qp::SpmcRing<qp::MarketEvent, 1024, 1, /*UseHeap=*/true>;

// Scaling: N rings merged in timestamp order, N a compile-time template
// parameter (real-world N is fixed per composition — one leg per venue —
// never a runtime count, so this mirrors how it's actually instantiated,
// same convention as bench_spmc_ring.cpp's NumConsumers<N> tiers).
template <std::size_t N>
void BM_BacktestInProcessTransport_NRings(benchmark::State& state) {
    std::array<Ring, N>        rings;
    std::array<Ring*, N>       ring_ptrs;
    std::array<std::size_t, N> consumers{};
    for (std::size_t i = 0; i < N; ++i) ring_ptrs[i] = &rings[i];

    qp::ControlChannel<1>                                         control;
    auto                                                          idx = control.attach();
    qp::engine::transport::BacktestInProcessTransport<Ring, N, 1> transport(ring_ptrs, consumers,
                                                                            control, idx);

    qp::Timestamp ts = 0;
    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) rings[i].push(make_trade(ts++));
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

// Populated depth, N=2 (the only shape anything real uses — not
// re-answering the scaling question above, just isolating next()'s own
// move-vs-copy on a realistic BookDiff).
void BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff(benchmark::State& state) {
    constexpr std::size_t      N = 2;
    std::array<Ring, N>        rings;
    std::array<Ring*, N>       ring_ptrs;
    std::array<std::size_t, N> consumers{};
    for (std::size_t i = 0; i < N; ++i) ring_ptrs[i] = &rings[i];

    qp::ControlChannel<1>                                         control;
    auto                                                          idx = control.attach();
    qp::engine::transport::BacktestInProcessTransport<Ring, N, 1> transport(ring_ptrs, consumers,
                                                                            control, idx);

    qp::Timestamp ts = 0;
    for (auto _ : state) {
        for (std::size_t i = 0; i < N; ++i) rings[i].push(make_book_diff(ts++));
        for (std::size_t i = 0; i < N; ++i) {
            auto out = transport.next();
            benchmark::DoNotOptimize(out);
        }
    }
}

BENCHMARK(BM_BacktestInProcessTransport_TwoRingsPopulatedBookDiff);

// Real contention: N producer threads, each owning one leg's ring, racing
// the consumer thread's next() calls on a shared transport.
// The retry is bounded.
// StopFlushesBufferedEventsInsteadOfStallingForever in the test file needs
// one). A benchmark's steady-state loop has no natural place to signal
// that once per repetition, so this bounds the spin instead of hanging on
// that one tail item.
template <std::size_t N>
void BM_BacktestInProcessTransport_NRingsContended(benchmark::State& state) {
    static std::array<Ring, N>  rings;
    static std::array<Ring*, N> ring_ptrs = [] {
        std::array<Ring*, N> ptrs;
        for (std::size_t i = 0; i < N; ++i) ptrs[i] = &rings[i];
        return ptrs;
    }();
    static std::array<std::size_t, N> consumers{};
    static qp::ControlChannel<1>      control;
    static std::size_t                control_idx = control.attach();
    static qp::engine::transport::BacktestInProcessTransport<Ring, N, 1> transport(
        ring_ptrs, consumers, control, control_idx);

    constexpr int kMaxSpins   = 2'000'000;
    bool          is_consumer = state.thread_index() == static_cast<int>(N);

    if (is_consumer) {
        while (transport.next()) {  // drain whatever a prior repetition left buffered
        }
    }

    if (is_consumer) {
        for (auto _ : state) {
            for (std::size_t n = 0; n < N; ++n) {
                std::optional<qp::MarketEvent> out;
                for (int spin = 0; spin < kMaxSpins && !(out = transport.next()); ++spin) {
                }
                benchmark::DoNotOptimize(out);
            }
        }
    } else {
        auto          leg = static_cast<std::size_t>(state.thread_index());
        qp::Timestamp ts  = 0;
        for (auto _ : state) {
            rings[leg].push(make_trade(ts));
            ts += static_cast<qp::Timestamp>(N);  // legs interleave, never collide on ts
        }
    }
}

BENCHMARK(BM_BacktestInProcessTransport_NRingsContended<2>)->Threads(3);
BENCHMARK(BM_BacktestInProcessTransport_NRingsContended<4>)->Threads(5);
BENCHMARK(BM_BacktestInProcessTransport_NRingsContended<8>)->Threads(9);

}  // namespace
