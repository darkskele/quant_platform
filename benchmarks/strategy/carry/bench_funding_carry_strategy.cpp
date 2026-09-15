#include <benchmark/benchmark.h>

#include <array>

#include "funding_carry_strategy.hpp"
#include "portfolio.hpp"
#include "support/market_event_builders.hpp"

namespace {

using qp::strategy::carry::Config;
using qp::strategy::carry::FundingCarryStrategy;

constexpr qp::SymbolId               kSymbol        = 1;
constexpr qp::Market                 kSpotMarket    = qp::Market::BinanceUsdm;
constexpr qp::Market                 kFuturesMarket = qp::Market::BinanceCoinm;
constexpr std::array<std::size_t, 2> kCounts{2, 2};
using Book = qp::Portfolio;

Config make_config() {
    return Config{.symbol             = kSymbol,
                  .spot_market        = kSpotMarket,
                  .futures_market     = kFuturesMarket,
                  .target_qty         = 2.0,
                  .entry_funding_rate = 0.0001,
                  .exit_funding_rate  = 0.0};
}

// Entry path
void BM_FundingCarryStrategy_EntersPosition(benchmark::State& state) {
    Book                 portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    auto                 event = qp::test::make_funding(kSymbol, 0, 0.0002, kFuturesMarket);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_EntersPosition);

// Hold path
void BM_FundingCarryStrategy_HoldsPosition(benchmark::State& state) {
    Book                 portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .market = kSpotMarket, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .market = kFuturesMarket, .qty = 2.0});
    auto event = qp::test::make_funding(kSymbol, 0, 0.00005, kFuturesMarket);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_HoldsPosition);

void BM_FundingCarryStrategy_FlattensPosition(benchmark::State& state) {
    Book                 portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .market = kSpotMarket, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .market = kFuturesMarket, .qty = 2.0});
    auto event = qp::test::make_funding(kSymbol, 0, 0.0, kFuturesMarket);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_FlattensPosition);

void BM_FundingCarryStrategy_IgnoresNonMatchingEvent(benchmark::State& state) {
    Book                 portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    auto                 event = qp::test::make_trade(kSymbol, 0, 100.0);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_IgnoresNonMatchingEvent);

}  // namespace
