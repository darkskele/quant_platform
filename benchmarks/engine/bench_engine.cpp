#include <benchmark/benchmark.h>

#include "clock.hpp"
#include "engine.hpp"
#include "execution_gateway.hpp"
#include "last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "sim_clock.hpp"
#include "sim_execution.hpp"
#include "strategy.hpp"
#include "support/fake_transport.hpp"
#include "support/market_event_builders.hpp"
#include "support/risk_gate_doubles.hpp"
#include "support/strategy_doubles.hpp"
#include "types.hpp"

namespace {

using qp::test::AlwaysApproveRiskGate;
using qp::test::AlwaysIntentStrategy;
using qp::test::InfiniteTransport;
using qp::test::NoopStrategy;
using TestExec = qp::execution::SimExecution<qp::execution::LastTradeMatcher>;

// Floor: no Intent, no risk/submit/fill work — just transport pull, clock
// advance, exec.on_market_event, and one empty pool round-trip.
void BM_Engine_StepOneNoopStrategyOneWorker(benchmark::State& state) {
    qp::Engine<InfiniteTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 1, NoopStrategy>
        engine{InfiniteTransport{qp::test::make_funding(1, 0, 0.0)}, qp::SimClock{}, TestExec{},
               AlwaysApproveRiskGate{}, NoopStrategy{}};
    for (auto _ : state) engine.step();
}

BENCHMARK(BM_Engine_StepOneNoopStrategyOneWorker);

// Full cycle: Intent every step, risk-approved, submitted, matcher fills
// against the seeded Trade price, Portfolio updated.
void BM_Engine_StepOneStrategyFullPipeline(benchmark::State& state) {
    qp::Engine<InfiniteTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 1,
               AlwaysIntentStrategy>
        engine{InfiniteTransport{qp::test::make_trade(1, 0, 100.0)}, qp::SimClock{}, TestExec{},
               AlwaysApproveRiskGate{}, AlwaysIntentStrategy{}};
    for (auto _ : state) engine.step();
}

BENCHMARK(BM_Engine_StepOneStrategyFullPipeline);

// Same floor case, 2 strategies — 1 worker (sequential) vs 2 (parallel) —
// same comparison bench_round_robin_pool.cpp makes for the bare pool, now
// with the rest of step() (clock/exec/risk/drain) wrapped around it.
void BM_Engine_StepTwoNoopStrategiesOneWorker(benchmark::State& state) {
    qp::Engine<InfiniteTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 1, NoopStrategy,
               NoopStrategy>
        engine{InfiniteTransport{qp::test::make_funding(1, 0, 0.0)},
               qp::SimClock{},
               TestExec{},
               AlwaysApproveRiskGate{},
               NoopStrategy{},
               NoopStrategy{}};
    for (auto _ : state) engine.step();
}

BENCHMARK(BM_Engine_StepTwoNoopStrategiesOneWorker);

void BM_Engine_StepTwoNoopStrategiesTwoWorkers(benchmark::State& state) {
    qp::Engine<InfiniteTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 2, NoopStrategy,
               NoopStrategy>
        engine{InfiniteTransport{qp::test::make_funding(1, 0, 0.0)},
               qp::SimClock{},
               TestExec{},
               AlwaysApproveRiskGate{},
               NoopStrategy{},
               NoopStrategy{}};
    for (auto _ : state) engine.step();
}

BENCHMARK(BM_Engine_StepTwoNoopStrategiesTwoWorkers);

}  // namespace
