#include <benchmark/benchmark.h>

#include <array>

#include "basic_risk_gate.hpp"
#include "portfolio.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"

namespace {

using qp::Market;
using qp::risk::basic::BasicRiskGate;
using qp::risk::basic::BasicRiskGateConfig;
using qp::test::make_fill;
using qp::test::make_trade;

constexpr std::array<std::size_t, 2> kCounts{2, 2};
using Book = qp::Portfolio;

// check() approves within the cap — no existing position, no clamping.
void BM_BasicRiskGate_ApprovesWhenFlat(benchmark::State& state) {
    Book          portfolio{kCounts};
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};
    qp::Intent    intent{.symbol = 1, .market = Market::BinanceUsdm, .target_position = 2.0};

    for (auto _ : state) {
        auto decision = gate.check(intent);
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ApprovesWhenFlat);

// check() clamps to max_position_qty — the Resized path.
void BM_BasicRiskGate_ClampsAndResizes(benchmark::State& state) {
    Book          portfolio{kCounts};
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 1.0, .max_drawdown = 1000.0},
                       portfolio};
    qp::Intent    intent{.symbol = 1, .market = Market::BinanceUsdm, .target_position = 10.0};

    for (auto _ : state) {
        auto decision = gate.check(intent);
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ClampsAndResizes);

// on_tick() with no drawdown, common case. 
void BM_BasicRiskGate_OnTickNoDrawdown(benchmark::State& state) {
    Book          portfolio{kCounts};
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};

    for (auto _ : state) {
        auto orders = gate.on_tick();
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickNoDrawdown);

// on_tick() when it actually trips, the full kill-switch path.
void BM_BasicRiskGate_OnTickTripsAndFlattens(benchmark::State& state) {
    Book portfolio{kCounts};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 3.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0,
                                   /*fee=*/0.0, /*market=*/Market::BinanceUsdm));
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, /*price=*/100.0, /*order_id=*/2,
                                   /*ts=*/0,
                                   /*fee=*/0.0, /*market=*/Market::BinanceCoinm));
    portfolio.apply_mark_price(
        make_trade(1, 0, /*price=*/20.0, 1.0, qp::Side::Buy, Market::BinanceUsdm));
    portfolio.apply_mark_price(
        make_trade(1, 0, /*price=*/20.0, 1.0, qp::Side::Buy, Market::BinanceCoinm));

    for (auto _ : state) {
        BasicRiskGate gate{
            BasicRiskGateConfig{.max_position_qty = 10.0,
                                .max_drawdown     = 50.0,
                                .tracked = {{1, Market::BinanceUsdm}, {1, Market::BinanceCoinm}}},
            portfolio};
        gate.check(qp::Intent{.symbol = 1, .market = Market::BinanceUsdm, .target_position = 3.0});
        gate.check(
            qp::Intent{.symbol = 1, .market = Market::BinanceCoinm, .target_position = -2.0});

        auto orders = gate.on_tick();
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickTripsAndFlattens);

}  // namespace
