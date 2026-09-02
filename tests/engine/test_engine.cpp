#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "clock.hpp"
#include "control_channel.hpp"
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
#include "support/strategy_doubles.hpp"
#include "transport.hpp"
#include "types.hpp"

using qp::test::AlwaysApproveRiskGate;
using qp::test::AlwaysFlattenRiskGate;
using qp::test::make_funding;
using qp::test::make_mark_price_kline;
using qp::test::make_trade;
using qp::test::NoopStrategy;

namespace {

constexpr std::array<std::size_t, 1> kCounts{2};
using Portfolio = qp::Portfolio<kCounts>;

struct FakeTransport {
    std::vector<qp::MarketEvent> events;
    std::size_t                  index = 0;

    std::optional<qp::MarketEvent> next() {
        if (index >= events.size()) return std::nullopt;
        return events[index++];
    }

    void flush() noexcept {}
};

/// Fires once, on the first on_event call, then goes quiet — enough to
/// prove one Intent reaches RiskGate/exec/Portfolio without needing a real
/// strategy.
struct SingleIntentStrategy {
    qp::SymbolId symbol;
    qp::Qty      target_position;
    bool         fired = false;
    qp::Intent   intent_{};

    std::span<const qp::Intent> on_event(const qp::MarketEvent&) {
        if (fired) return {};
        fired   = true;
        intent_ = qp::Intent{.symbol = symbol, .target_position = target_position};
        return {&intent_, 1};
    }

    std::span<const qp::Intent> on_timer(qp::Timestamp) { return {}; }
};

struct CountingStrategy {
    int* calls;

    std::span<const qp::Intent> on_event(const qp::MarketEvent&) {
        ++*calls;
        return {};
    }

    std::span<const qp::Intent> on_timer(qp::Timestamp) { return {}; }
};

using TestExec = qp::execution::sim::SimExecution<
    qp::execution::sim::matcher::last_trade::LastTradeMatcher<Portfolio>, Portfolio>;
using SingleTest = qp::engine::Engine<FakeTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate,
                                      SingleIntentStrategy, Portfolio>;
using CountTest  = qp::engine::Engine<FakeTransport, qp::SimClock, TestExec, AlwaysApproveRiskGate,
                                      CountingStrategy, Portfolio>;

}  // namespace

static_assert(qp::engine::transport::Transport<FakeTransport>);
static_assert(qp::strategy::Strategy<SingleIntentStrategy>);
static_assert(qp::strategy::Strategy<CountingStrategy>);
static_assert(qp::risk::RiskGate<AlwaysApproveRiskGate>);

TEST(Engine, StepReturnsFalseWhenTransportExhausted) {
    Portfolio  portfolio;
    SingleTest engine{FakeTransport{},
                      qp::SimClock{},
                      TestExec{},
                      AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
                      portfolio};

    EXPECT_FALSE(engine.step());
}

TEST(Engine, TradeThenIntentApprovedFillsAndUpdatesPortfolio) {
    std::vector<qp::MarketEvent> events{make_trade(1, 1000, 100.0)};
    Portfolio                    portfolio;
    SingleTest                   engine{FakeTransport{events},
                      qp::SimClock{},
                      TestExec{},
                      AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
                      portfolio};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(portfolio.position(1, 0), 2.0);
    EXPECT_FALSE(engine.step());  // transport now exhausted
}

TEST(Engine, NoPriceSeenYetRejectsAndLeavesPortfolioUnaffected) {
    std::vector<qp::MarketEvent> events{make_funding(1, 1000, 0.0)};  // not a Trade -> no price
    Portfolio                    portfolio;
    SingleTest                   engine{FakeTransport{events},
                      qp::SimClock{},
                      TestExec{},
                      AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
                      portfolio};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(portfolio.position(1, 0), 0.0);
}

TEST(Engine, RunDrainsEveryEventInTheTransport) {
    int                          calls = 0;
    std::vector<qp::MarketEvent> events{
        make_funding(1, 1000, 0.0),
        make_funding(1, 2000, 0.0),
        make_funding(1, 3000, 0.0),
    };
    Portfolio portfolio;
    CountTest engine{FakeTransport{events},   qp::SimClock{},           TestExec{},
                     AlwaysApproveRiskGate{}, CountingStrategy{&calls}, portfolio};

    while (engine.step()) {
    }
    EXPECT_EQ(calls, 3);
}

// run() on its own thread processes everything the transport hands it, then
// exits once Stop arrives on the control channel — the shape a top-level
// app drives it in (see BacktestBase::run).
TEST(Engine, RunProcessesEveryEventThenExitsOnControlStop) {
    int                          calls = 0;
    std::vector<qp::MarketEvent> events{
        make_funding(1, 1000, 0.0),
        make_funding(1, 2000, 0.0),
        make_funding(1, 3000, 0.0),
    };
    Portfolio portfolio;
    CountTest engine{FakeTransport{events},   qp::SimClock{},           TestExec{},
                     AlwaysApproveRiskGate{}, CountingStrategy{&calls}, portfolio};

    qp::ControlChannel<1> control;
    std::size_t           consumer = control.attach();
    std::thread           runner([&] { engine.run(control, consumer); });

    // Give the loop time to drain the (finite) transport, then stop it.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    control.request_stop();
    runner.join();

    EXPECT_EQ(calls, 3);
}

TEST(Engine, FundingEventSettlesAgainstCurrentPositionBeforeStrategyReacts) {
    std::vector<qp::MarketEvent> events{
        make_trade(1, 1000, 100.0),
        make_mark_price_kline(1, 1500, /*close=*/100.0),
        make_funding(1, 2000, 0.0001),
    };
    Portfolio  portfolio;
    SingleTest engine{FakeTransport{events},
                      qp::SimClock{},
                      TestExec{},
                      AlwaysApproveRiskGate{},
                      SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
                      portfolio};

    EXPECT_TRUE(engine.step());  // Trade -> intent -> fill: position 2, cash -200 - taker fee
    EXPECT_TRUE(engine.step());  // MarkPriceKline -> sets the price funding settles against
    EXPECT_TRUE(engine.step());  // Funding -> settles against that position
    // -(2*100) buy cost, -0.08 LastTradeMatcher's taker fee (100*2*0.0004), -0.02 funding.
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0 - 0.08 - 0.02);
}

// Book is a reference, not owned: two single-Strategy Engines pointed at
// the same Portfolio see each other's fills, so an account-level view
// (equity, combined position) is correct across both.
TEST(Engine, SiblingEnginesShareOnePortfolioAcrossBothStrategiesFills) {
    std::vector<qp::MarketEvent> events_a{make_trade(1, 1000, 100.0)};
    std::vector<qp::MarketEvent> events_b{make_trade(1, 1000, 100.0)};
    Portfolio                    portfolio;
    SingleTest                   engine_a{FakeTransport{events_a},
                        qp::SimClock{},
                        TestExec{},
                        AlwaysApproveRiskGate{},
                        SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
                        portfolio};
    SingleTest                   engine_b{FakeTransport{events_b},
                        qp::SimClock{},
                        TestExec{},
                        AlwaysApproveRiskGate{},
                        SingleIntentStrategy{.symbol = 1, .target_position = 3.0},
                        portfolio};

    EXPECT_TRUE(engine_a.step());
    EXPECT_TRUE(engine_b.step());

    EXPECT_EQ(portfolio.position(1, 0), 5.0);  // both engines' fills landed on the one shared book
}

// step()'s two order-generating paths are independent: strategy.on_event()
// -> risk.check() -> submit(), and risk.on_tick() -> submit() directly, no
// Intent involved. NoopStrategy never emits an Intent, so this isolates
// the second path — proving Engine actually forwards a RiskGate's
// autonomous orders through submit()/fills()/drain_outcomes(), not just
// that BasicRiskGate's own on_tick() produces the right Order in isolation.
TEST(Engine, RiskGateOnTickOrdersAreSubmittedAndAppliedToThePortfolio) {
    std::vector<qp::MarketEvent> events{make_trade(1, 1000, 100.0)};
    Portfolio                    portfolio;
    qp::engine::Engine<FakeTransport, qp::SimClock, TestExec, AlwaysFlattenRiskGate, NoopStrategy,
                       Portfolio>
        engine{FakeTransport{events},
               qp::SimClock{},
               TestExec{},
               AlwaysFlattenRiskGate{
                   .order = qp::Order{.id = 1, .symbol = 1, .side = qp::Side::Sell, .qty = 2.0}},
               NoopStrategy{},
               portfolio};

    EXPECT_TRUE(engine.step());  // same-step Trade primes the matcher's price for the fill

    EXPECT_EQ(portfolio.position(1, 0), -2.0);
}
