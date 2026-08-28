#include <benchmark/benchmark.h>

#include "last_trade_matcher.hpp"
#include "sim_execution.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::LastTradeMatcher;
using qp::execution::SimExecution;

namespace {

constexpr SymbolId kSymbol = 1;
constexpr VenueId  kVenue  = 0;

// Tandem: submit() + next_outcome() together, single thread — no real
// concurrency (SimExecution is Engine-thread-only, same as Portfolio).
// This is the plumbing SimExecution adds on top of LastTradeMatcher's own
// on_market_event/try_fill (bench_last_trade_matcher.cpp): a std::queue
// push/pop around the matcher's fill decision.
void BM_SimExecution_SubmitNextOutcome(benchmark::State& state) {
    SimExecution<LastTradeMatcher> exec;
    MarketEvent                    trade;
    trade.kind   = EventKind::Trade;
    trade.symbol = kSymbol;
    trade.venue  = kVenue;
    trade.price  = 100.0;
    exec.on_market_event(trade);

    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .venue = kVenue, .qty = 1.0};
    for (auto _ : state) {
        exec.submit(order, 0);
        auto outcome = exec.next_outcome();
        benchmark::DoNotOptimize(outcome);
    }
}

BENCHMARK(BM_SimExecution_SubmitNextOutcome);

}  // namespace
