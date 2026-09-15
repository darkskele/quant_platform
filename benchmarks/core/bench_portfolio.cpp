#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>

#include "portfolio.hpp"
#include "types.hpp"

using namespace qp;

namespace {

constexpr SymbolId                   kSymbol = 1;
constexpr Market                     kMarket = Market::BinanceUsdm;
constexpr std::array<std::size_t, 1> kCounts{2};

Portfolio make_portfolio() { return Portfolio{kCounts}; }

void BM_Portfolio_ApplyFill(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    Fill fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0};
    for (auto _ : state) {
        portfolio.apply_fill(fill);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFill);

void BM_Portfolio_ApplyFunding(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0});
    FundingEvent event;
    event.base.symbol  = kSymbol;
    event.base.market  = kMarket;
    event.funding_rate = 0.0001;
    // No mark_price to set.
    for (auto _ : state) {
        portfolio.apply_funding(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFunding);

void BM_Portfolio_ApplyMarkPrice(benchmark::State& state) {
    Portfolio  portfolio = make_portfolio();
    TradeEvent event;
    event.base.symbol = kSymbol;
    event.base.market = kMarket;
    event.price       = 100.0;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPrice);

// MarkPriceKlineEvent's branch writes both mark_price_ and
// funding_mark_price.
void BM_Portfolio_ApplyMarkPriceFromMarkPriceKline(benchmark::State& state) {
    Portfolio           portfolio = make_portfolio();
    MarkPriceKlineEvent event;
    event.base.symbol = kSymbol;
    event.base.market = kMarket;
    event.close       = 100.0;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPriceFromMarkPriceKline);

// Floor.
void BM_Portfolio_ApplyMarkPriceIgnoresBookDiff(benchmark::State& state) {
    Portfolio     portfolio = make_portfolio();
    BookDiffEvent event;
    event.base.symbol = kSymbol;
    event.base.market = kMarket;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPriceIgnoresBookDiff);

void BM_Portfolio_Position(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0});
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.position(kSymbol, kMarket));
}

BENCHMARK(BM_Portfolio_Position);

// The one op with real algorithmic cost.
void BM_Portfolio_Equity(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0});
    TradeEvent mark;
    mark.base.symbol = kSymbol;
    mark.base.market = kMarket;
    mark.price       = 105.0;
    portfolio.apply_mark_price(mark);
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.equity());
}

BENCHMARK(BM_Portfolio_Equity);

}  // namespace
