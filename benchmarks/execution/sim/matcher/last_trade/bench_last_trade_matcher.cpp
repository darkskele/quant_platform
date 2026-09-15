#include <benchmark/benchmark.h>

#include <array>
#include <vector>

#include "last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::matcher::last_trade::LastTradeMatcher;

namespace {

constexpr SymbolId                   kSymbol = 1;
constexpr Market                     kMarket = Market::BinanceUsdm;
constexpr std::array<std::size_t, 1> kCounts{2};
using Book = Portfolio;

Book make_book() { return Book{kCounts}; }

MarketEvent make_trade_event() {
    TradeEvent ev;
    ev.base.symbol = kSymbol;
    ev.base.market = kMarket;
    ev.price       = 100.0;
    ev.qty         = 1.0;
    return ev;
}

void BM_LastTradeMatcher_OnMarketEvent(benchmark::State& state) {
    Book             book = make_book();
    LastTradeMatcher matcher{book};
    auto             ev = make_trade_event();
    for (auto _ : state) {
        matcher.on_market_event(ev);
        benchmark::DoNotOptimize(matcher);
    }
}

BENCHMARK(BM_LastTradeMatcher_OnMarketEvent);

void BM_LastTradeMatcher_OnMarketEventDiscardsPopulatedBookDiff(benchmark::State& state) {
    Book                    book = make_book();
    LastTradeMatcher        matcher{book};
    constexpr int           kLevels = 20;
    std::vector<PriceLevel> bids(kLevels, PriceLevel{.price = 100.0, .qty = 1.0});
    std::vector<PriceLevel> asks(kLevels, PriceLevel{.price = 101.0, .qty = 1.0});
    auto ev = qp::test::make_book_diff(kSymbol, /*ts=*/0, /*first_seq=*/0, /*seq=*/0,
                                       /*prev_seq=*/0, bids, asks, kMarket);
    for (auto _ : state) {
        matcher.on_market_event(ev);
        benchmark::DoNotOptimize(matcher);
    }
}

BENCHMARK(BM_LastTradeMatcher_OnMarketEventDiscardsPopulatedBookDiff);

void BM_LastTradeMatcher_TryFillFills(benchmark::State& state) {
    Book             book = make_book();
    LastTradeMatcher matcher{book};
    matcher.on_market_event(make_trade_event());
    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .market = kMarket, .qty = 1.0};
    for (auto _ : state) {
        auto outcome = matcher.try_fill(order, 0);
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_LastTradeMatcher_TryFillFills);

void BM_LastTradeMatcher_TryFillRejects(benchmark::State& state) {
    Book             book = make_book();
    LastTradeMatcher matcher{book};
    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .market = kMarket, .qty = 1.0};
    for (auto _ : state) {
        auto outcome = matcher.try_fill(order, 0);
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_LastTradeMatcher_TryFillRejects);

}  // namespace
