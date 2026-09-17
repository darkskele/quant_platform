#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>
#include <vector>

#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "exchange.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

constexpr SlotOffset  kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset  kMarket   = 0;
constexpr Timestamp   kWeekNs   = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
constexpr std::size_t kSymbols  = 10;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    for (std::size_t i = 0; i < kSymbols; ++i) {
        sub.add(ExchangeId::Binance, kMarket, std::string{"S"} + std::to_string(i));
    }
    return std::move(sub).build();
}

using Book = Portfolio;
using Cost = HalfSpreadLinearImpact;

Book make_book() { return Book{make_subscription()}; }

std::vector<CostRow> build_table(std::size_t weeks_per_symbol) {
    std::vector<CostRow> rows;
    rows.reserve(kSymbols * weeks_per_symbol);
    for (std::size_t sym = 0; sym < kSymbols; ++sym) {
        for (std::size_t w = 0; w < weeks_per_symbol; ++w) {
            rows.push_back(CostRow{
                .exchange            = kExchange,
                .market              = kMarket,
                .symbol              = static_cast<SlotOffset>(sym),
                .week_start_ns       = static_cast<Timestamp>(w) * kWeekNs,
                .half_spread_bps     = 1.0 + 0.01 * static_cast<double>(w % 20),
                .impact_bps_per_unit = 0.0,
                .taker_fee_bps       = 4.0,
            });
        }
    }
    return rows;
}

Order make_order(SlotOffset symbol) {
    return Order{.id       = 0,
                 .exchange = kExchange,
                 .market   = kMarket,
                 .symbol   = symbol,
                 .side     = Side::Buy,
                 .qty      = 1.0};
}

}  // namespace

void BM_HalfSpreadLinearImpact_Price(benchmark::State& state) {
    Book            book = make_book();
    Cost            cost{book, build_table(150)};
    Order           o   = make_order(3);
    const Price     ref = 50000.0;
    const Timestamp ts  = 75 * kWeekNs;
    for (auto _ : state) {
        auto out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_Price);

void BM_HalfSpreadLinearImpact_PriceMiss(benchmark::State& state) {
    std::vector<CostRow> rows{CostRow{.exchange            = kExchange,
                                      .market              = kMarket,
                                      .symbol              = 0,
                                      .week_start_ns       = 0,
                                      .half_spread_bps     = 1.0,
                                      .impact_bps_per_unit = 0.0,
                                      .taker_fee_bps       = 4.0}};
    Book                 book = make_book();
    Cost                 cost{book, std::move(rows)};
    Order                o   = make_order(7);
    const Price          ref = 50000.0;
    const Timestamp      ts  = kWeekNs;
    for (auto _ : state) {
        auto out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_PriceMiss);

void BM_HalfSpreadLinearImpact_PriceRotatingSymbol(benchmark::State& state) {
    Book            book = make_book();
    Cost            cost{book, build_table(150)};
    const Price     ref = 50000.0;
    const Timestamp ts  = 75 * kWeekNs;
    std::size_t     i   = 0;
    for (auto _ : state) {
        Order o   = make_order(static_cast<SlotOffset>(i % kSymbols));
        auto  out = cost.price(o, ref, ts);
        benchmark::DoNotOptimize(out);
        ++i;
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_PriceRotatingSymbol);

void BM_HalfSpreadLinearImpact_Construction(benchmark::State& state) {
    Book book = make_book();
    auto rows = build_table(150);
    for (auto _ : state) {
        auto copy = rows;
        Cost cost{book, std::move(copy)};
        benchmark::DoNotOptimize(cost);
    }
}

BENCHMARK(BM_HalfSpreadLinearImpact_Construction);
