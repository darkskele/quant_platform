#include <benchmark/benchmark.h>

#include <array>

#include "funding_carry_strategy.hpp"
#include "portfolio.hpp"
#include "support/market_event_builders.hpp"

namespace {

using qp::strategy::carry::Config;
using qp::strategy::carry::FundingCarryStrategy;

constexpr qp::SymbolId               kSymbol       = 1;
constexpr qp::VenueId                kSpotVenue    = 0;
constexpr qp::VenueId                kFuturesVenue = 1;
constexpr std::array<std::size_t, 2> kCounts{2, 2};
using Book = qp::Portfolio<kCounts>;

Config make_config() {
    return Config{.symbol             = kSymbol,
                  .spot_venue         = kSpotVenue,
                  .futures_venue      = kFuturesVenue,
                  .target_qty         = 2.0,
                  .entry_funding_rate = 0.0001,
                  .exit_funding_rate  = 0.0};
}

// Entry path: funding_rate clears the threshold, two Intents built/returned.
// Not latency-critical live (funding ticks every ~8h) — this matters for
// backtest sweep throughput, called once per historical Funding event per
// swept config.
void BM_FundingCarryStrategy_EntersPosition(benchmark::State& state) {
    Book                       portfolio;
    FundingCarryStrategy<Book> strategy{make_config(), portfolio};
    auto event = qp::test::make_funding(kSymbol, 0, 0.0002, 0.0, kFuturesVenue);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_EntersPosition);

// Hold path: same shape (still 2 Intents), reads the current position off
// portfolio_ instead of a constant — the branch a real run takes most
// often, since funding rarely crosses a threshold on every tick.
void BM_FundingCarryStrategy_HoldsPosition(benchmark::State& state) {
    Book                       portfolio;
    FundingCarryStrategy<Book> strategy{make_config(), portfolio};
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .venue = kSpotVenue, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .venue = kFuturesVenue, .qty = 2.0});
    auto event = qp::test::make_funding(kSymbol, 0, 0.00005, 0.0, kFuturesVenue);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_HoldsPosition);

// Reject path: wrong kind/symbol/venue — the early return every other case
// pays on top of, isolated here as the floor.
void BM_FundingCarryStrategy_IgnoresNonMatchingEvent(benchmark::State& state) {
    Book                       portfolio;
    FundingCarryStrategy<Book> strategy{make_config(), portfolio};
    auto                       event = qp::test::make_trade(kSymbol, 0, 100.0);

    for (auto _ : state) {
        auto intents = strategy.on_event(event);
        benchmark::DoNotOptimize(intents);
    }
}

BENCHMARK(BM_FundingCarryStrategy_IgnoresNonMatchingEvent);

}  // namespace
