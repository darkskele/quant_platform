#include <benchmark/benchmark.h>

#include "exchange.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "subscription.hpp"
#include "types.hpp"

using namespace qp;
using qp::execution::sim::SimExecution;
using qp::execution::sim::matcher::last_trade::LastTradeMatcher;

namespace {

constexpr std::uint16_t kExchange = static_cast<std::uint16_t>(ExchangeId::Binance);
constexpr std::uint16_t kMarket   = 0;
constexpr std::uint16_t kSymbol   = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    sub.add(ExchangeId::Binance, kMarket, "B");
    return std::move(sub).build();
}

using Book = Portfolio;
using Exec = SimExecution<LastTradeMatcher>;

MarketEvent trade_event(Price price) {
    MarketEvent ev;
    ev.base = {
        .kind = EventKind::Trade, .exchange = kExchange, .market = kMarket, .symbol = kSymbol};
    ev.payload = TradeEvent{.price = price};
    return ev;
}

Order sample_order() {
    return Order{.id       = 1,
                 .exchange = kExchange,
                 .market   = kMarket,
                 .symbol   = kSymbol,
                 .side     = Side::Buy,
                 .qty      = 1.0};
}

void BM_SimExecution_SubmitFills(benchmark::State& state) {
    Book book{make_subscription()};
    Exec exec{book, LastTradeMatcher{book}};
    exec.on_market_event(trade_event(100.0));

    Order order = sample_order();
    for (auto _ : state) {
        exec.reset_outcomes();
        exec.submit(order, 0);
        auto fills = exec.fills();
        benchmark::DoNotOptimize(fills);
    }
}

BENCHMARK(BM_SimExecution_SubmitFills);

void BM_SimExecution_SubmitRejects(benchmark::State& state) {
    Book  book{make_subscription()};
    Exec  exec{book, LastTradeMatcher{book}};
    Order order = sample_order();
    for (auto _ : state) {
        exec.reset_outcomes();
        exec.submit(order, 0);
        auto rejects = exec.rejects();
        benchmark::DoNotOptimize(rejects);
    }
}

BENCHMARK(BM_SimExecution_SubmitRejects);

}  // namespace
