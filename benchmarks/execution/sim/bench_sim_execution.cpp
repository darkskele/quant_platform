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
constexpr Market                     kMarket = Market::BinanceUsdm;
constexpr std::array<std::size_t, 1> kCounts{2};
using Book = Portfolio;
using Exec = SimExecution<LastTradeMatcher>;

void BM_SimExecution_SubmitFills(benchmark::State& state) {
    Book       book{kCounts};
    Exec       exec{book, LastTradeMatcher{book}};
    TradeEvent trade;
    trade.base.symbol = kSymbol;
    trade.base.market = kMarket;
    trade.price       = 100.0;
    exec.on_market_event(trade);

    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .market = kMarket, .qty = 1.0};
    for (auto _ : state) {
        exec.reset_outcomes();
        exec.submit(order, 0);
        auto fills = exec.fills();
        benchmark::DoNotOptimize(fills);
    }
}

BENCHMARK(BM_SimExecution_SubmitFills);

// Reject-path counterpart
void BM_SimExecution_SubmitRejects(benchmark::State& state) {
    Book  book{kCounts};
    Exec  exec{book, LastTradeMatcher{book}};
    Order order{.id = 1, .symbol = kSymbol, .side = Side::Buy, .market = kMarket, .qty = 1.0};
    for (auto _ : state) {
        exec.reset_outcomes();
        exec.submit(order, 0);
        auto rejects = exec.rejects();
        benchmark::DoNotOptimize(rejects);
    }
}

BENCHMARK(BM_SimExecution_SubmitRejects);

}  // namespace
