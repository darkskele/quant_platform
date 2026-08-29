#include <benchmark/benchmark.h>

#include <vector>

#include "last_trade_matcher.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::matcher::last_trade::LastTradeMatcher;

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

// Isolation: on_market_event() — the array write per (symbol, venue) index.
// DoNotOptimize(matcher) is load-bearing here, not decorative: the write
// only touches matcher's own last_price_ array, which nothing reads
// afterward and which never escapes this function, so without it the
// optimizer can (and does) prove the whole call dead and delete it —
// TryFillFills/TryFillRejects below don't need this because DoNotOptimize
// on their returned outcome already keeps try_fill() alive.
void BM_LastTradeMatcher_OnMarketEvent(benchmark::State& state) {
    LastTradeMatcher matcher;
    auto             ev = make_trade_event();
    for (auto _ : state) {
        matcher.on_market_event(ev);
        benchmark::DoNotOptimize(matcher);
    }
}

BENCHMARK(BM_LastTradeMatcher_OnMarketEvent);

// Isolation, populated depth: a BookDiff — real bid/ask levels, unlike
// Trade above which never carries any — flowing through on_market_event()
// only to be discarded immediately (kind != Trade). This is what
// on_market_event(const MarketEvent&) actually buys over the by-value
// signature it used to have: no copy of bids/asks for a field this class
// never reads. kLevels matches a realistic partial-depth update (Binance's
// 20-level partial book stream), not the always-empty vectors every other
// benchmark here uses (Trade/Funding never carry book levels at all, so
// they can't exercise this cost regardless of level count).
void BM_LastTradeMatcher_OnMarketEventDiscardsPopulatedBookDiff(benchmark::State& state) {
    LastTradeMatcher        matcher;
    constexpr int           kLevels = 20;
    std::vector<PriceLevel> bids(kLevels, PriceLevel{.price = 100.0, .qty = 1.0});
    std::vector<PriceLevel> asks(kLevels, PriceLevel{.price = 101.0, .qty = 1.0});
    auto ev = qp::test::make_book_diff(kSymbol, /*ts=*/0, /*first_seq=*/0, /*seq=*/0,
                                       /*prev_seq=*/0, bids, asks, kVenue);
    for (auto _ : state) {
        matcher.on_market_event(ev);
        benchmark::DoNotOptimize(matcher);
    }
}

BENCHMARK(BM_LastTradeMatcher_OnMarketEventDiscardsPopulatedBookDiff);

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
