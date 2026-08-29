#include <benchmark/benchmark.h>

#include "basic_risk_gate.hpp"
#include "portfolio.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"

namespace {

using qp::risk::basic::BasicRiskGate;
using qp::risk::basic::BasicRiskGateConfig;
using qp::test::make_fill;
using qp::test::make_trade;

// check() approves within the cap — no existing position, no clamping.
void BM_BasicRiskGate_ApprovesWhenFlat(benchmark::State& state) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{BasicRiskGateConfig{}, portfolio};
    qp::Intent                   intent{.symbol = 1, .venue = 0, .target_position = 2.0};

    for (auto _ : state) {
        auto decision = gate.check(intent);
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ApprovesWhenFlat);

// check() clamps to max_position_qty — the Resized path.
void BM_BasicRiskGate_ClampsAndResizes(benchmark::State& state) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 1.0, .max_drawdown = 1000.0}, portfolio};
    qp::Intent intent{.symbol = 1, .venue = 0, .target_position = 10.0};

    for (auto _ : state) {
        auto decision = gate.check(intent);
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ClampsAndResizes);

// on_tick() with no drawdown — the overwhelmingly common case: Engine::step()
// calls this on every processed event, not just when a Strategy fires, and a
// kill switch trips at most once per run (if ever). This is the cost that
// actually accumulates across a backtest.
void BM_BasicRiskGate_OnTickNoDrawdown(benchmark::State& state) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    for (auto _ : state) {
        auto orders = gate.on_tick();
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickNoDrawdown);

// on_tick() when it actually trips: the full kill-switch path — the
// equity() scan, the tracked-positions walk, building each flatten Order.
// tripped_ latches (fires at most once per gate's lifetime), so each
// iteration uses a fresh gate tracking two (symbol, venue) legs (spot +
// futures shape, matching FundingCarryStrategy) against a shared Portfolio
// pre-loaded with a large enough decline to trip on the first call. Gate
// construction + the two check() calls are cheap (~ns) and left inside the
// timed region rather than paused around — PauseTiming/ResumeTiming are
// themselves slow enough (see bench_spsc_queue.cpp) that pausing every
// iteration would leak into the very cost being measured.
void BM_BasicRiskGate_OnTickTripsAndFlattens(benchmark::State& state) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0,
                                   /*fee=*/0.0, /*venue=*/0));
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 3.0, /*price=*/100.0, /*order_id=*/2, /*ts=*/0,
                                   /*fee=*/0.0, /*venue=*/1));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/20.0, 1.0, qp::Side::Buy, 0));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/20.0, 1.0, qp::Side::Buy, 1));

    for (auto _ : state) {
        BasicRiskGate<qp::Portfolio> gate{
            BasicRiskGateConfig{
                .max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {{1, 0}, {1, 1}}},
            portfolio};
        gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 2.0});
        gate.check(qp::Intent{.symbol = 1, .venue = 1, .target_position = 3.0});

        auto orders = gate.on_tick();
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickTripsAndFlattens);

}  // namespace
