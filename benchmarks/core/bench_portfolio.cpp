#include <benchmark/benchmark.h>

#include <array>

#include "portfolio.hpp"
#include "types.hpp"

using namespace qp;

namespace {

constexpr SymbolId                   kSymbol = 1;
constexpr MarketId                    kMarket  = 0;
constexpr std::array<std::size_t, 1> kCounts{2};

// Isolation only: Portfolio's write side (apply_fill/apply_funding/
// apply_mark_price) is Engine-thread-only, sequenced strictly before
// Strategy/RiskGate's own reads through their held const Book& (D37) —
// never touched from two threads at once within one Engine, so there's no
// tandem/contention tier that would measure anything real here.

void BM_Portfolio_ApplyFill(benchmark::State& state) {
    Portfolio<kCounts> portfolio;
    Fill fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0};
    for (auto _ : state) {
        portfolio.apply_fill(fill);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFill);

void BM_Portfolio_ApplyFunding(benchmark::State& state) {
    Portfolio<kCounts> portfolio;
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0});
    FundingEvent event;
    event.symbol       = kSymbol;
    event.market        = kMarket;
    event.funding_rate = 0.0001;
    // No mark_price to set — FundingEvent no longer carries one (settlement
    // now reads Portfolio's own funding_mark_price_, fed by
    // MarkPriceKlineEvent). Left unseeded (defaults to 0.0): doesn't change
    // what's being timed here, apply_funding does the same multiply either
    // way, just against a different operand.
    for (auto _ : state) {
        portfolio.apply_funding(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyFunding);

void BM_Portfolio_ApplyMarkPrice(benchmark::State& state) {
    Portfolio<kCounts> portfolio;
    TradeEvent         event;
    event.symbol = kSymbol;
    event.market  = kMarket;
    event.price  = 100.0;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPrice);

// MarkPriceKlineEvent's branch writes both mark_price_ and
// funding_mark_price_ — two array writes, not TradeEvent/KlineEvent's one.
void BM_Portfolio_ApplyMarkPriceFromMarkPriceKline(benchmark::State& state) {
    Portfolio<kCounts>  portfolio;
    MarkPriceKlineEvent event;
    event.symbol = kSymbol;
    event.market  = kMarket;
    event.close  = 100.0;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPriceFromMarkPriceKline);

// Floor: BookDiff/BookSnapshot carry no scalar price, so this falls through
// every if constexpr branch untouched — the cost every other event's own
// write pays on top of.
void BM_Portfolio_ApplyMarkPriceIgnoresBookDiff(benchmark::State& state) {
    Portfolio<kCounts> portfolio;
    BookDiffEvent      event;
    event.symbol = kSymbol;
    event.market  = kMarket;
    for (auto _ : state) {
        portfolio.apply_mark_price(event);
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Portfolio_ApplyMarkPriceIgnoresBookDiff);

void BM_Portfolio_Position(benchmark::State& state) {
    Portfolio<kCounts> portfolio;
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0});
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.position(kSymbol, kMarket));
}

BENCHMARK(BM_Portfolio_Position);

// The one op with real algorithmic cost: a full kMaxSymbols*kMaxVenues
// (64*8 = 512 entry) linear scan, by design (portfolio.hpp: called at most
// once per Engine::step(), cheap next to that cadence — not worth
// incremental bookkeeping on every write for a value read this rarely).
void BM_Portfolio_Equity(benchmark::State& state) {
    Portfolio<kCounts> portfolio;
    portfolio.apply_fill(
        Fill{.symbol = kSymbol, .side = Side::Buy, .market = kMarket, .price = 100.0, .qty = 1.0});
    TradeEvent mark;
    mark.symbol = kSymbol;
    mark.market  = kMarket;
    mark.price  = 105.0;
    portfolio.apply_mark_price(mark);
    for (auto _ : state) benchmark::DoNotOptimize(portfolio.equity());
}

BENCHMARK(BM_Portfolio_Equity);

}  // namespace
