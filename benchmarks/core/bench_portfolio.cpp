#include <benchmark/benchmark.h>

#include "exchange.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "types.hpp"

using namespace qp;

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

Portfolio make_portfolio() { return Portfolio{make_subscription()}; }

Fill sample_fill() {
    return Fill{.exchange = kExchange,
                .market   = kMarket,
                .symbol   = kSymbol,
                .side     = Side::Buy,
                .price    = 100.0,
                .qty      = 1.0};
}

MarketEvent trade_event(Price price) {
    MarketEvent ev;
    ev.base = {
        .kind = EventKind::Trade, .exchange = kExchange, .market = kMarket, .symbol = kSymbol};
    ev.payload = TradeEvent{.price = price};
    return ev;
}

MarketEvent funding_event(double rate) {
    MarketEvent ev;
    ev.base = {
        .kind = EventKind::Funding, .exchange = kExchange, .market = kMarket, .symbol = kSymbol};
    ev.payload = FundingEvent{.funding_rate = rate};
    return ev;
}

MarketEvent mark_price_kline_event(Price close) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::MarkPriceKline,
                  .exchange = kExchange,
                  .market   = kMarket,
                  .symbol   = kSymbol};
    ev.payload = MarkPriceKlineEvent{.close = close};
    return ev;
}

MarketEvent book_depth_event() {
    MarketEvent ev;
    ev.base = {
        .kind = EventKind::BookDepth, .exchange = kExchange, .market = kMarket, .symbol = kSymbol};
    ev.payload = BookDepthEvent{};
    return ev;
}

void BM_Portfolio_ApplyFill(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    Fill      fill      = sample_fill();
    for (auto _ : state) {
        portfolio.apply_fill(fill);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFill);

void BM_Portfolio_ApplyFunding(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    portfolio.apply_fill(sample_fill());
    auto event = funding_event(0.0001);
    for (auto _ : state) {
        portfolio.apply_funding(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFunding);

void BM_Portfolio_ApplyMarkPrice(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    auto      event     = trade_event(100.0);
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPrice);

void BM_Portfolio_ApplyMarkPriceFromMarkPriceKline(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    auto      event     = mark_price_kline_event(100.0);
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPriceFromMarkPriceKline);

void BM_Portfolio_ApplyMarkPriceIgnoresBookDepth(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    auto      event     = book_depth_event();
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPriceIgnoresBookDepth);

void BM_Portfolio_Position(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    portfolio.apply_fill(sample_fill());
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.position(kExchange, kMarket, kSymbol));
}

BENCHMARK(BM_Portfolio_Position);

void BM_Portfolio_Equity(benchmark::State& state) {
    Portfolio portfolio = make_portfolio();
    portfolio.apply_fill(sample_fill());
    portfolio.apply_mark_price(trade_event(105.0));
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.equity());
}

BENCHMARK(BM_Portfolio_Equity);

}  // namespace
