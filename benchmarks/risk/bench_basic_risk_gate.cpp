#include <benchmark/benchmark.h>

#include "basic_risk_gate.hpp"
#include "portfolio.hpp"

namespace {

using qp::risk::BasicRiskGate;
using qp::risk::BasicRiskGateConfig;

// check() approves within the cap — no existing position, no clamping.
void BM_BasicRiskGate_ApprovesWhenFlat(benchmark::State& state) {
    qp::Portfolio portfolio;
    BasicRiskGate gate{BasicRiskGateConfig{}};
    qp::Intent    intent{.symbol = 1, .venue = 0, .target_position = 2.0};

    for (auto _ : state) {
        auto decision = gate.check(intent, portfolio.view());
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ApprovesWhenFlat);

// check() clamps to max_position_qty — the Resized path.
void BM_BasicRiskGate_ClampsAndResizes(benchmark::State& state) {
    qp::Portfolio portfolio;
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 1.0, .max_drawdown = 1000.0}};
    qp::Intent    intent{.symbol = 1, .venue = 0, .target_position = 10.0};

    for (auto _ : state) {
        auto decision = gate.check(intent, portfolio.view());
        benchmark::DoNotOptimize(decision);
    }
}

BENCHMARK(BM_BasicRiskGate_ClampsAndResizes);

// on_tick() with no drawdown — the overwhelmingly common case: Engine::step()
// calls this on every processed event, not just when a Strategy fires, and a
// kill switch trips at most once per run (if ever). This is the cost that
// actually accumulates across a backtest.
void BM_BasicRiskGate_OnTickNoDrawdown(benchmark::State& state) {
    qp::Portfolio portfolio;
    BasicRiskGate gate{BasicRiskGateConfig{}};

    for (auto _ : state) {
        auto orders = gate.on_tick(portfolio.view());
        benchmark::DoNotOptimize(orders);
    }
}

BENCHMARK(BM_BasicRiskGate_OnTickNoDrawdown);

}  // namespace
