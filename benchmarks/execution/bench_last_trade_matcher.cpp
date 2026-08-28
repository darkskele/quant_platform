#include <benchmark/benchmark.h>

#include "last_trade_matcher.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::LastTradeMatcher;

namespace {

constexpr SymbolId kSymbol = 1;
constexpr VenueId  kVenue  = 0;

MarketEvent make_trade_event() {
    MarketEvent ev;
    ev.kind   = EventKind::Trade;
    ev.symbol = kSymbol;
    ev.venue  = kVenue;
    ev.price  = 100.0;
    ev.qty    = 1.0;
    return ev;
}

// Isolation: on_market_event() — the unordered_map insert/update per
// (symbol, venue) key.
void BM_LastTradeMatcher_OnMarketEvent(benchmark::State& state) {
    LastTradeMatcher matcher;
    auto             ev = make_trade_event();
    for (auto _ : state) matcher.on_market_event(ev);
}

BENCHMARK(BM_LastTradeMatcher_OnMarketEvent);

// Isolation: try_fill(), fill path — a price has been seen for this
// (symbol, venue).
void BM_LastTradeMatcher_TryFillFills(benchmark::State& state) {
    LastTradeMatcher matcher;
    matcher.on_market_event(make_trade_event());
    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .qty = 1.0};
    for (auto _ : state) {
        auto outcome = matcher.try_fill(order, 0);
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_LastTradeMatcher_TryFillFills);

// Isolation: try_fill(), reject path — no Trade ever seen for this
// (symbol, venue), the early "can't fill" floor every other case pays on
// top of.
void BM_LastTradeMatcher_TryFillRejects(benchmark::State& state) {
    LastTradeMatcher matcher;
    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .qty = 1.0};
    for (auto _ : state) {
        auto outcome = matcher.try_fill(order, 0);
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_LastTradeMatcher_TryFillRejects);

}  // namespace
