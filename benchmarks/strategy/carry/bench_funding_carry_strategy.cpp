#include <benchmark/benchmark.h>

#include "exchange.hpp"
#include "funding_carry_strategy.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"

namespace {

using qp::ExchangeId;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::strategy::carry::Config;
using qp::strategy::carry::FundingCarryStrategy;
using qp::strategy::carry::Leg;

constexpr std::uint16_t kExchange      = static_cast<std::uint16_t>(ExchangeId::Binance);
constexpr std::uint16_t kFuturesMarket = 0;
constexpr std::uint16_t kSpotMarket    = 1;
constexpr std::uint16_t kSymbol        = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kFuturesMarket, "A");
    sub.add(ExchangeId::Binance, kFuturesMarket, "B");
    sub.add(ExchangeId::Binance, kSpotMarket, "A");
    sub.add(ExchangeId::Binance, kSpotMarket, "B");
    return std::move(sub).build();
}

using Book = qp::Portfolio;

Config make_config() {
    return Config{.futures            = Leg{kExchange, kFuturesMarket, kSymbol},
                  .spot               = Leg{kExchange, kSpotMarket, kSymbol},
                  .target_qty         = 2.0,
                  .entry_funding_rate = 0.0001,
                  .exit_funding_rate  = 0.0};
}

void BM_FundingCarryStrategy_EntersPosition(benchmark::State& state) {
    Book                 portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    auto                 event = qp::test::make_funding(kSymbol, 0, 0.0002, kFuturesMarket);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_EntersPosition);

void BM_FundingCarryStrategy_HoldsPosition(benchmark::State& state) {
    Book                 portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kSpotMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Buy,
                                  .qty      = 2.0});
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kFuturesMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Sell,
                                  .qty      = 2.0});
    auto event = qp::test::make_funding(kSymbol, 0, 0.00005, kFuturesMarket);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_HoldsPosition);

void BM_FundingCarryStrategy_FlattensPosition(benchmark::State& state) {
    Book                 portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kSpotMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Buy,
                                  .qty      = 2.0});
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kFuturesMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Sell,
                                  .qty      = 2.0});
    auto event = qp::test::make_funding(kSymbol, 0, 0.0, kFuturesMarket);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_FlattensPosition);

void BM_FundingCarryStrategy_IgnoresNonMatchingEvent(benchmark::State& state) {
    Book                 portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    auto                 event = qp::test::make_trade(kSymbol, 0, 100.0);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_IgnoresNonMatchingEvent);

}  // namespace
