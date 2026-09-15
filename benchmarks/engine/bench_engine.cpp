#include <benchmark/benchmark.h>

#include <array>

#include "engine.hpp"
#include "execution_gateway.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
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
using qp::test::SeedThenSteadyStateTransport;
constexpr std::array<std::size_t, 1> kCounts{2};
using Book     = qp::Portfolio;
using TestExec =
    qp::execution::sim::SimExecution<qp::execution::sim::matcher::last_trade::LastTradeMatcher>;

// Floor: no Intent, no risk/submit/fill work — just transport pull, clock
// advance, exec.on_market_event, and one direct Strategy call.
void BM_Engine_StepOneNoopStrategy(benchmark::State& state) {
    Book portfolio{kCounts};
    qp::engine::Engine<InfiniteTransport, TestExec, AlwaysApproveRiskGate, NoopStrategy>
        engine{InfiniteTransport{qp::test::make_funding(1, 0, 0.0)},

               TestExec{portfolio,
                        qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
               AlwaysApproveRiskGate{}, NoopStrategy{}, portfolio};
    // DoNotOptimize(portfolio), not just the discarded step() bool: step()'s
    // real effects are writes into portfolio's memory (apply_fill/
    // apply_funding/apply_mark_price) that nothing here ever reads back —
    // every object involved is header-only and fully visible in this TU, so
    // without this the optimizer can (and does — verified by comparing
    // against StepOneStrategyFullPipeline below, which should cost more
    // than this floor and doesn't without the marker) prove the whole call
    // chain dead and strip it to noise.
    for (auto _ : state) {
        engine.step();
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Engine_StepOneNoopStrategy);

// Full cycle: Intent every step, risk-approved, submitted, matcher fills
// against the seeded Trade price, Portfolio updated (apply_fill via
// drain_outcomes). Trade-driven — see StepFundingEventFullPipeline below
// for the apply_funding-driven shape a real funding-reactive strategy
// actually runs.
void BM_Engine_StepOneStrategyFullPipeline(benchmark::State& state) {
    Book portfolio{kCounts};
    qp::engine::Engine<InfiniteTransport, TestExec, AlwaysApproveRiskGate, AlwaysIntentStrategy>
        engine{InfiniteTransport{qp::test::make_trade(1, 0, 100.0)},

               TestExec{portfolio,
                        qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
               AlwaysApproveRiskGate{}, AlwaysIntentStrategy{}, portfolio};
    for (auto _ : state) {
        engine.step();
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Engine_StepOneStrategyFullPipeline);

// Full cycle, Funding-driven: apply_funding() (settling against whatever
// position drain_outcomes just applied) + intent -> risk -> submit ->
// matcher fill -> drain_outcomes(apply_fill), every step — the shape
// FundingCarryStrategy actually runs in production (it only ever reacts
// to Funding events, never Trade ones, unlike StepOneStrategyFullPipeline
// above). SeedThenSteadyStateTransport seeds the matcher with a Trade
// price once so submit() fills instead of rejecting, then every
// subsequent step is the Funding event under measurement.
void BM_Engine_StepFundingEventFullPipeline(benchmark::State& state) {
    Book portfolio{kCounts};
    qp::engine::Engine<SeedThenSteadyStateTransport, TestExec, AlwaysApproveRiskGate,
                       AlwaysIntentStrategy>
        engine{SeedThenSteadyStateTransport{qp::test::make_trade(1, 0, 100.0),
                                            qp::test::make_funding(1, 0, 0.0001)},

               TestExec{portfolio,
                        qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
               AlwaysApproveRiskGate{}, AlwaysIntentStrategy{}, portfolio};
    for (auto _ : state) {
        engine.step();
        benchmark::DoNotOptimize(portfolio);
    }
}

BENCHMARK(BM_Engine_StepFundingEventFullPipeline);

}  // namespace
