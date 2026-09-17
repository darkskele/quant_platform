#include <benchmark/benchmark.h>

#include <vector>

#include "exchange.hpp"
#include "last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::matcher::last_trade::LastTradeMatcher;

namespace {

constexpr std::uint16_t kExchange = static_cast<std::uint16_t>(ExchangeId::Binance);
constexpr std::uint16_t kMarket   = 0;
constexpr std::uint16_t kSymbol   = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    sub.add(ExchangeId::Binance, kMarket, "B");
    return std::move(sub).build();
}

using Book = Portfolio;

Book make_book() { return Book{make_subscription()}; }

MarketEvent make_trade_event() {
    MarketEvent ev;
    ev.base = {
        .kind = EventKind::Trade, .exchange = kExchange, .market = kMarket, .symbol = kSymbol};
    ev.payload = TradeEvent{.price = 100.0, .qty = 1.0};
    return ev;
}

Order sample_order() {
    return Order{.id       = 1,
                 .exchange = kExchange,
                 .market   = kMarket,
                 .symbol   = kSymbol,
                 .side     = Side::Buy,
                 .qty      = 1.0};
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
    auto ev = qp::test::make_book_diff(kSymbol, 0, 0, 0, 0, bids, asks, kMarket, kExchange);
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
    Order order = sample_order();
    for (auto _ : state) {
        auto outcome = matcher.try_fill(order, 0);
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_LastTradeMatcher_TryFillFills);

void BM_LastTradeMatcher_TryFillRejects(benchmark::State& state) {
    Book             book = make_book();
    LastTradeMatcher matcher{book};
    Order            order = sample_order();
    for (auto _ : state) {
        auto outcome = matcher.try_fill(order, 0);
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_LastTradeMatcher_TryFillRejects);

}  // namespace
