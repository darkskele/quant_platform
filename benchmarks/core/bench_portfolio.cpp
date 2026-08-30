#include <benchmark/benchmark.h>

#include "portfolio.hpp"
#include "types.hpp"

using namespace qp;

namespace {

constexpr SymbolId kSymbol = 1;
constexpr VenueId  kVenue  = 0;

// Isolation only: Portfolio's write side (apply_fill/apply_funding/
// apply_mark_price) is Engine-thread-only, sequenced strictly before
// Strategy/RiskGate's own reads through their held const Book& (D37) —
// never touched from two threads at once within one Engine, so there's no
// tandem/contention tier that would measure anything real here.

void BM_Portfolio_ApplyFill(benchmark::State& state) {
    Portfolio portfolio;
    Fill fill{.symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .price = 100.0, .qty = 1.0};
    for (auto _ : state) {
        portfolio.apply_fill(fill);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFill);

void BM_Portfolio_ApplyFunding(benchmark::State& state) {
    Portfolio portfolio;
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .price = 100.0, .qty = 1.0});
    FundingEvent event;
    event.symbol       = kSymbol;
    event.venue        = kVenue;
    event.mark_price   = 100.0;
    event.funding_rate = 0.0001;
    for (auto _ : state) {
        portfolio.apply_funding(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFunding);

void BM_Portfolio_ApplyMarkPrice(benchmark::State& state) {
    Portfolio  portfolio;
    TradeEvent event;
    event.symbol = kSymbol;
    event.venue  = kVenue;
    event.price  = 100.0;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPrice);

void BM_Portfolio_Position(benchmark::State& state) {
    Portfolio portfolio;
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .price = 100.0, .qty = 1.0});
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.position(kSymbol, kVenue));
}

BENCHMARK(BM_Portfolio_Position);

// The one op with real algorithmic cost: a full kMaxSymbols*kMaxVenues
// (64*8 = 512 entry) linear scan, by design (portfolio.hpp: called at most
// once per Engine::step(), cheap next to that cadence — not worth
// incremental bookkeeping on every write for a value read this rarely).
void BM_Portfolio_Equity(benchmark::State& state) {
    Portfolio portfolio;
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .price = 100.0, .qty = 1.0});
    TradeEvent mark;
    mark.symbol = kSymbol;
    mark.venue  = kVenue;
    mark.price  = 105.0;
    portfolio.apply_mark_price(mark);
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.equity());
}

BENCHMARK(BM_Portfolio_Equity);

}  // namespace
