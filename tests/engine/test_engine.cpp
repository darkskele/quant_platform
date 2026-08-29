#include <gtest/gtest.h>

#include <optional>
#include <span>
#include <vector>

#include "clock.hpp"
#include "engine.hpp"
#include "execution_gateway.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "sim_clock.hpp"
#include "sim_execution.hpp"
#include "strategy.hpp"
#include "support/market_event_builders.hpp"
#include "support/risk_gate_doubles.hpp"
#include "transport.hpp"
#include "types.hpp"

using qp::test::AlwaysApproveRiskGate;
using qp::test::make_funding;
using qp::test::make_trade;

namespace {

struct FakeTransport {
    std::vector<qp::MarketEvent> events;
    std::size_t                  index = 0;

    std::optional<qp::MarketEvent> next() {
        if (index >= events.size()) return std::nullopt;
        return events[index++];
    }
};

/// Fires once, on the first on_event call, then goes quiet — enough to
/// prove one Intent reaches RiskGate/exec/Portfolio without needing a real
/// strategy.
struct SingleIntentStrategy {
    qp::SymbolId symbol;
    qp::Qty      target_position;
    bool         fired = false;
    qp::Intent   intent_{};

    std::span<const qp::Intent> on_event(const qp::MarketEvent&, qp::StateView) {
        if (fired) return {};
        fired   = true;
        intent_ = qp::Intent{.symbol = symbol, .target_position = target_position};
        return {&intent_, 1};
    }

    std::span<const qp::Intent> on_timer(qp::Timestamp, qp::StateView) { return {}; }
};

struct CountingStrategy {
    int* calls;

    std::span<const qp::Intent> on_event(const qp::MarketEvent&, qp::StateView) {
        ++*calls;
        return {};
    }

    std::span<const qp::Intent> on_timer(qp::Timestamp, qp::StateView) { return {}; }
};

using TestExec =
    qp::execution::sim::SimExecution<qp::execution::sim::matcher::last_trade::LastTradeMatcher>;
using SingleTest = qp::Engine<FakeTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 1,
                              SingleIntentStrategy>;
using CountTest =
    qp::Engine<FakeTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 1, CountingStrategy>;
using TwoStratTest = qp::Engine<FakeTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate, 1,
                                SingleIntentStrategy, SingleIntentStrategy>;

}  // namespace

static_assert(qp::transport::Transport<FakeTransport>);
static_assert(qp::strategy::Strategy<SingleIntentStrategy>);
static_assert(qp::strategy::Strategy<CountingStrategy>);
static_assert(qp::risk::RiskGate<AlwaysApproveRiskGate>);

TEST(Engine, StepReturnsFalseWhenTransportExhausted) {
    SingleTest engine{FakeTransport{}, qp::SimClock{}, TestExec{}, AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0}};

    EXPECT_FALSE(engine.step());
}

TEST(Engine, TradeThenIntentApprovedFillsAndUpdatesPortfolio) {
    std::vector<qp::MarketEvent> events{make_trade(1, 1000, 100.0)};
    SingleTest engine{FakeTransport{events}, qp::SimClock{}, TestExec{}, AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0}};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(engine.view().position(1, 0), 2.0);
    EXPECT_FALSE(engine.step());  // transport now exhausted
}

TEST(Engine, NoPriceSeenYetRejectsAndLeavesPortfolioUnaffected) {
    std::vector<qp::MarketEvent> events{make_funding(1, 1000, 0.0)};  // not a Trade -> no price
    SingleTest engine{FakeTransport{events}, qp::SimClock{}, TestExec{}, AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0}};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(engine.view().position(1, 0), 0.0);
}

TEST(Engine, RunDrainsEveryEventInTheTransport) {
    int                          calls = 0;
    std::vector<qp::MarketEvent> events{
        make_funding(1, 1000, 0.0),
        make_funding(1, 2000, 0.0),
        make_funding(1, 3000, 0.0),
    };
    CountTest engine{FakeTransport{events}, qp::SimClock{}, TestExec{}, AlwaysApproveRiskGate{},
                     CountingStrategy{&calls}};

    engine.run();
    EXPECT_EQ(calls, 3);
}

TEST(Engine, MultipleStrategiesEachContributeAndFillDeterministically) {
    std::vector<qp::MarketEvent> events{make_trade(1, 1000, 100.0)};
    // 2 strategies, 1 worker (NumWorkers < strategy count) -> both run on
    // the same thread, sequentially, still both contribute.
    TwoStratTest engine{FakeTransport{events},
                        qp::SimClock{},
                        TestExec{},
                        AlwaysApproveRiskGate{},
                        SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
                        SingleIntentStrategy{.symbol = 1, .target_position = 3.0}};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(engine.view().position(1, 0), 5.0);  // both orders filled: 2 + 3
}

TEST(Engine, FundingEventSettlesAgainstCurrentPositionBeforeStrategyReacts) {
    std::vector<qp::MarketEvent> events{
        make_trade(1, 1000, 100.0),
        make_funding(1, 2000, 0.0001, 100.0),
    };
    SingleTest engine{FakeTransport{events}, qp::SimClock{}, TestExec{}, AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0}};

    EXPECT_TRUE(engine.step());  // Trade -> intent -> fill: position 2, cash -200 - taker fee
    EXPECT_TRUE(engine.step());  // Funding -> settles against that position
    // -(2*100) buy cost, -0.08 LastTradeMatcher's taker fee (100*2*0.0004), -0.02 funding.
    EXPECT_DOUBLE_EQ(engine.view().cash(), -200.0 - 0.08 - 0.02);
}
