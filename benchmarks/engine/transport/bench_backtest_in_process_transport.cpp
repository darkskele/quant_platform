#include <benchmark/benchmark.h>

#include <array>
#include <memory>
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
// Monotonically increasing timestamps across all N rings each round, so
// every next() call finds a winner immediately — isolates next()'s own
// merge cost, not the empty-ring stall path. N=2 is the only shape
// anything real uses today (apps/backtest: one spot + one futures leg);
// N=4/8/16 exist to answer "does the O(N) lookahead scan in next() start
// costing something before N gets anywhere near what this system would
// ever actually run" (docs/strategy.md: "a handful, not hundreds").
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
// move-vs-copy on a realistic BookDiff). Cheap either way now: the ring
// holds MarketEvent directly (SpmcRing::try_pop's copy into lookahead_ is
// a shared_ptr refcount bump, not a deep BookLevels copy — see
// core/types.hpp), and next()'s own return moves out of lookahead_, not a
// second copy on top.
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

}  // namespace
