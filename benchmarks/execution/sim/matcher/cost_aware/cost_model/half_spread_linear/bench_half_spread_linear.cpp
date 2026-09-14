#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "portfolio.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

// 10 symbols on one market, matches the funding-carry universe.
constexpr std::array<std::size_t, 1> kCounts{10};
using Book = Portfolio<kCounts>;
using Cost = HalfSpreadLinearImpact<Book>;

constexpr Timestamp   kWeekNs  = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
constexpr std::size_t kSymbols = 10;

// A realistic funding-carry cost table shape: 10 symbols x N weeks each.
// Rows are shuffled at insertion, then the constructor groups/sorts them.
std::vector<CostRow> build_table(std::size_t weeks_per_symbol) {
    std::vector<CostRow> rows;
    rows.reserve(kSymbols * weeks_per_symbol);
    for (std::size_t sym = 0; sym < kSymbols; ++sym) {
        for (std::size_t w = 0; w < weeks_per_symbol; ++w) {
            rows.push_back(CostRow{
                .symbol              = static_cast<SymbolId>(sym),
                .market               = 0,
                .week_start_ns       = static_cast<Timestamp>(w) * kWeekNs,
                .half_spread_bps     = 1.0 + 0.01 * static_cast<double>(w % 20),
                .impact_bps_per_unit = 0.0,
                .taker_fee_bps       = 4.0,
            });
        }
    }
    return rows;
}

}  // namespace

// price() hot path with a full-scale funding-carry table (10 symbols,
// ~150 weeks each). One Book::index computation, one binary search over
// a small contiguous span.
void BM_HalfSpreadLinearImpact_Price(benchmark::State& state) {
    Cost            cost{build_table(/*weeks_per_symbol=*/150)};
    Order           o{.id = 0, .symbol = 3, .side = Side::Buy, .market = 0, .qty = 1.0};
    const Price     ref = 50000.0;
    const Timestamp ts  = 75 * kWeekNs;
    for (auto _ : state) {
        auto out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_Price);

// price() miss path: (symbol, market) is out of the table's populated
// slots. Slot lookup still O(1), returns empty span, no binary search.
void BM_HalfSpreadLinearImpact_PriceMiss(benchmark::State& state) {
    // Table only carries symbol 0. Look up symbol 7.
    std::vector<CostRow> rows{CostRow{.symbol              = 0,
                                      .market               = 0,
                                      .week_start_ns       = 0,
                                      .half_spread_bps     = 1.0,
                                      .impact_bps_per_unit = 0.0,
                                      .taker_fee_bps       = 4.0}};
    Cost                 cost{std::move(rows)};
    Order                o{.id = 0, .symbol = 7, .side = Side::Buy, .market = 0, .qty = 1.0};
    const Price          ref = 50000.0;
    const Timestamp      ts  = kWeekNs;
    for (auto _ : state) {
        auto out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_PriceMiss);

// price() as symbol varies across all slots, to catch any cache warmth
// artifacts from always hitting the same slot.
void BM_HalfSpreadLinearImpact_PriceRotatingSymbol(benchmark::State& state) {
    Cost            cost{build_table(/*weeks_per_symbol=*/150)};
    const Price     ref = 50000.0;
    const Timestamp ts  = 75 * kWeekNs;
    std::size_t     i   = 0;
    for (auto _ : state) {
        Order o{.id     = 0,
                .symbol = static_cast<SymbolId>(i % kSymbols),
                .side   = Side::Buy,
                .market  = 0,
                .qty    = 1.0};
        auto  out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
        ++i;
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_PriceRotatingSymbol);

// Construction cost: grouping, sorting per-slot, span setup. Runs once at
// backtest startup, so this is size-vs-time not hot-path perf.
void BM_HalfSpreadLinearImpact_Construction(benchmark::State& state) {
    auto rows = build_table(/*weeks_per_symbol=*/150);
    for (auto _ : state) {
        auto copy = rows;  // reset input
        Cost cost{std::move(copy)};
        benchmark::DoNotOptimize(cost);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_Construction);
