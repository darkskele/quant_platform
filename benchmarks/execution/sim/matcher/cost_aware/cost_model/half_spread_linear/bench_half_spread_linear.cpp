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
using Book = Portfolio;
using Cost = HalfSpreadLinearImpact;

Book make_book() { return Book{kCounts}; }

constexpr Timestamp   kWeekNs  = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
constexpr std::size_t kSymbols = 10;

// A realistic funding-carry cost table shape
std::vector<CostRow> build_table(std::size_t weeks_per_symbol) {
    std::vector<CostRow> rows;
    rows.reserve(kSymbols * weeks_per_symbol);
    for (std::size_t sym = 0; sym < kSymbols; ++sym) {
        for (std::size_t w = 0; w < weeks_per_symbol; ++w) {
            rows.push_back(CostRow{
                .symbol              = static_cast<SymbolId>(sym),
                .market              = Market::BinanceUsdm,
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

// price() hot path with a full-scale funding-carry table
// a small contiguous span.
void BM_HalfSpreadLinearImpact_Price(benchmark::State& state) {
    Book  book = make_book();
    Cost  cost{book, build_table(/*weeks_per_symbol=*/150)};
    Order o{.id = 0, .symbol = 3, .side = Side::Buy, .market = Market::BinanceUsdm, .qty = 1.0};
    const Price     ref = 50000.0;
    const Timestamp ts  = 75 * kWeekNs;
    for (auto _ : state) {
        auto out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_Price);

// price() miss path
void BM_HalfSpreadLinearImpact_PriceMiss(benchmark::State& state) {
    // Table only carries symbol 0. Look up symbol 7.
    std::vector<CostRow> rows{CostRow{.symbol              = 0,
                                      .market              = Market::BinanceUsdm,
                                      .week_start_ns       = 0,
                                      .half_spread_bps     = 1.0,
                                      .impact_bps_per_unit = 0.0,
                                      .taker_fee_bps       = 4.0}};
    Book                 book = make_book();
    Cost                 cost{book, std::move(rows)};
    Order o{.id = 0, .symbol = 7, .side = Side::Buy, .market = Market::BinanceUsdm, .qty = 1.0};
    const Price     ref = 50000.0;
    const Timestamp ts  = kWeekNs;
    for (auto _ : state) {
        auto out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_PriceMiss);

// price() as symbol varies across all slots
void BM_HalfSpreadLinearImpact_PriceRotatingSymbol(benchmark::State& state) {
    Book            book = make_book();
    Cost            cost{book, build_table(/*weeks_per_symbol=*/150)};
    const Price     ref = 50000.0;
    const Timestamp ts  = 75 * kWeekNs;
    std::size_t     i   = 0;
    for (auto _ : state) {
        Order o{.id     = 0,
                .symbol = static_cast<SymbolId>(i % kSymbols),
                .side   = Side::Buy,
                .market = Market::BinanceUsdm,
                .qty    = 1.0};
        auto  out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
        ++i;
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_PriceRotatingSymbol);

// Construction cost
void BM_HalfSpreadLinearImpact_Construction(benchmark::State& state) {
    Book book = make_book();
    auto rows = build_table(/*weeks_per_symbol=*/150);
    for (auto _ : state) {
        auto copy = rows;  // reset input
        Cost cost{book, std::move(copy)};
        benchmark::DoNotOptimize(cost);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_Construction);
