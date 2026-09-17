#include <benchmark/benchmark.h>

#include "basic_risk_gate.hpp"
#include "exchange.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"

namespace {

using qp::ExchangeId;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::risk::basic::BasicRiskGate;
using qp::risk::basic::BasicRiskGateConfig;
using qp::risk::basic::TrackedInstrument;
using qp::test::make_fill;
using qp::test::make_trade;

constexpr SlotOffset kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kUsdm     = 0;
constexpr SlotOffset kCoinm    = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kUsdm, "A");
    sub.add(ExchangeId::Binance, kUsdm, "B");
    sub.add(ExchangeId::Binance, kCoinm, "A");
    sub.add(ExchangeId::Binance, kCoinm, "B");
    return std::move(sub).build();
}

using Book = qp::Portfolio;

qp::Intent intent(SlotOffset market, SlotOffset symbol, qp::Qty target) {
    return qp::Intent{
        .exchange = kExchange, .market = market, .symbol = symbol, .target_position = target};
}

TrackedInstrument tracked(SlotOffset market, SlotOffset symbol) {
    return TrackedInstrument{.exchange = kExchange, .market = market, .symbol = symbol};
}

void BM_BasicRiskGate_ApprovesWhenFlat(benchmark::State& state) {
    Book          portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};
    auto          in = intent(kUsdm, 1, 2.0);

    for (auto _ : state) {
        auto decision = gate.check(in);
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ApprovesWhenFlat);

void BM_BasicRiskGate_ClampsAndResizes(benchmark::State& state) {
    Book          portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 1.0, .max_drawdown = 1000.0},
                       portfolio};
    auto          in = intent(kUsdm, 1, 10.0);

    for (auto _ : state) {
        auto decision = gate.check(in);
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ClampsAndResizes);

void BM_BasicRiskGate_OnTickNoDrawdown(benchmark::State& state) {
    Book          portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};

    for (auto _ : state) {
        auto orders = gate.on_tick();
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickNoDrawdown);

void BM_BasicRiskGate_OnTickTripsAndFlattens(benchmark::State& state) {
    Book portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 3.0, 100.0, 1, 0, 0.0, kUsdm));
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, 100.0, 2, 0, 0.0, kCoinm));
    portfolio.apply_mark_price(make_trade(1, 0, 20.0, 1.0, qp::Side::Buy, kUsdm));
    portfolio.apply_mark_price(make_trade(1, 0, 20.0, 1.0, qp::Side::Buy, kCoinm));

    for (auto _ : state) {
        BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 10.0,
                                               .max_drawdown     = 50.0,
                                               .tracked = {tracked(kUsdm, 1), tracked(kCoinm, 1)}},
                           portfolio};
        gate.check(intent(kUsdm, 1, 3.0));
        gate.check(intent(kCoinm, 1, -2.0));

        auto orders = gate.on_tick();
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickTripsAndFlattens);

}  // namespace
