#include <benchmark/benchmark.h>

#include "clock.hpp"
#include "engine.hpp"
#include "execution_gateway.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
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
using TestExec =
    qp::execution::sim::SimExecution<qp::execution::sim::matcher::last_trade::LastTradeMatcher>;

// Floor: no Intent, no risk/submit/fill work — just transport pull, clock
// advance, exec.on_market_event, and one direct Strategy call.
void BM_Engine_StepOneNoopStrategy(benchmark::State& state) {
    qp::Portfolio portfolio;
    qp::Engine<InfiniteTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, NoopStrategy,
               qp::Portfolio>
        engine{InfiniteTransport{qp::test::make_funding(1, 0, 0.0)},
               qp::SimClock{},
               TestExec{},
               AlwaysApproveRiskGate{},
               NoopStrategy{},
               portfolio};
    for (auto _ : state) engine.step();
}

BENCHMARK(BM_Engine_StepOneNoopStrategy);

// Full cycle: Intent every step, risk-approved, submitted, matcher fills
// against the seeded Trade price, Portfolio updated.
void BM_Engine_StepOneStrategyFullPipeline(benchmark::State& state) {
    qp::Portfolio portfolio;
    qp::Engine<InfiniteTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate,
               AlwaysIntentStrategy, qp::Portfolio>
        engine{InfiniteTransport{qp::test::make_trade(1, 0, 100.0)},
               qp::SimClock{},
               TestExec{},
               AlwaysApproveRiskGate{},
               AlwaysIntentStrategy{},
               portfolio};
    for (auto _ : state) engine.step();
}

BENCHMARK(BM_Engine_StepOneStrategyFullPipeline);

}  // namespace
