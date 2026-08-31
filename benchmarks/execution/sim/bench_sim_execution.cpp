#include <benchmark/benchmark.h>

#include <array>

#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::SimExecution;
using qp::execution::sim::matcher::last_trade::LastTradeMatcher;

namespace {

constexpr SymbolId                   kSymbol = 1;
constexpr VenueId                    kVenue  = 0;
constexpr std::array<std::size_t, 1> kCounts{2};
using Book = Portfolio<kCounts>;
using Exec = SimExecution<LastTradeMatcher<Book>, Book>;

// Tandem: reset_outcomes() + submit() + fills(), single thread — no real
// concurrency (SimExecution is Engine-thread-only, same as Portfolio).
// reset_outcomes(), not on_market_event(), is what belongs in the loop: it's
// the plumbing SimExecution itself adds (a ViewablePool push/reset behind
// the matcher's fill decision, no queue, no variant on the outcome side).
// on_market_event() is seeded once, outside the loop, so this isolates that
// plumbing from the matcher's own on_market_event()/try_fill() cost
// (already measured separately in bench_last_trade_matcher.cpp).
void BM_SimExecution_SubmitFills(benchmark::State& state) {
    Exec       exec;
    TradeEvent trade;
    trade.symbol = kSymbol;
    trade.venue  = kVenue;
    trade.price  = 100.0;
    exec.on_market_event(trade);

    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .qty = 1.0};
    for (auto _ : state) {
        exec.reset_outcomes();
        exec.submit(order, 0);
        auto fills = exec.fills();
        benchmark::DoNotOptimize(fills);
    }
}

BENCHMARK(BM_SimExecution_SubmitFills);

// Reject-path counterpart: no Trade ever seen, every submit() lands in
// rejects() instead.
void BM_SimExecution_SubmitRejects(benchmark::State& state) {
    Exec  exec;
    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .qty = 1.0};
    for (auto _ : state) {
        exec.reset_outcomes();
        exec.submit(order, 0);
        auto rejects = exec.rejects();
        benchmark::DoNotOptimize(rejects);
    }
}

BENCHMARK(BM_SimExecution_SubmitRejects);

}  // namespace
