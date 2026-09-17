#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "control_channel.hpp"
#include "engine.hpp"
#include "exchange.hpp"
#include "execution_gateway.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "sim_execution.hpp"
#include "strategy.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"
#include "support/risk_gate_doubles.hpp"
#include "support/strategy_doubles.hpp"
#include "transport.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::test::AlwaysApproveRiskGate;
using qp::test::AlwaysFlattenRiskGate;
using qp::test::make_funding;
using qp::test::make_mark_price_kline;
using qp::test::make_trade;
using qp::test::NoopStrategy;

namespace {

constexpr SlotOffset kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kMarket   = 0;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    sub.add(ExchangeId::Binance, kMarket, "B");
    return std::move(sub).build();
}

using Portfolio = qp::Portfolio;

struct FakeTransport {
    std::vector<qp::MarketEvent> events;
    std::size_t                  index = 0;

    std::optional<qp::engine::transport::EngineInput> next() {
        if (index >= events.size()) return std::nullopt;
        auto& e = events[index++];
        return qp::engine::transport::EngineInput{e.base.ts, e};
    }

    void flush() noexcept {}
};

struct SingleIntentStrategy {
    qp::SlotOffset symbol;
    qp::Qty        target_position;
    bool           fired = false;
    qp::Intent     intent_{};

    std::span<const qp::Intent> on_event(const qp::MarketEvent&) {
        if (fired) return {};
        fired   = true;
        intent_ = qp::Intent{.exchange        = kExchange,
                             .market          = kMarket,
                             .symbol          = symbol,
                             .target_position = target_position};
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

using TestExec =
    qp::execution::sim::SimExecution<qp::execution::sim::matcher::last_trade::LastTradeMatcher>;
using SingleTest =
    qp::engine::Engine<FakeTransport, TestExec, AlwaysApproveRiskGate, SingleIntentStrategy>;
using CountTest =
    qp::engine::Engine<FakeTransport, TestExec, AlwaysApproveRiskGate, CountingStrategy>;

}  // namespace

static_assert(qp::engine::transport::Transport<FakeTransport>);
static_assert(qp::strategy::Strategy<SingleIntentStrategy>);
static_assert(qp::strategy::Strategy<CountingStrategy>);
static_assert(qp::risk::RiskGate<AlwaysApproveRiskGate>);

TEST(Engine, StepReturnsFalseWhenTransportExhausted) {
    Portfolio  portfolio{make_subscription()};
    SingleTest engine{
        FakeTransport{},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
        portfolio};

    EXPECT_FALSE(engine.step());
}

TEST(Engine, TradeThenIntentApprovedFillsAndUpdatesPortfolio) {
    std::vector<qp::MarketEvent> events{make_trade(1, 1000, 100.0)};
    Portfolio                    portfolio{make_subscription()};
    SingleTest                   engine{
        FakeTransport{events},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
        portfolio};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(portfolio.position(kExchange, kMarket, 1), 2.0);
    EXPECT_FALSE(engine.step());
}

TEST(Engine, NoPriceSeenYetRejectsAndLeavesPortfolioUnaffected) {
    std::vector<qp::MarketEvent> events{make_funding(1, 1000, 0.0)};
    Portfolio                    portfolio{make_subscription()};
    SingleTest                   engine{
        FakeTransport{events},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
        portfolio};

    EXPECT_TRUE(engine.step());
    EXPECT_EQ(portfolio.position(kExchange, kMarket, 1), 0.0);
}

TEST(Engine, RunDrainsEveryEventInTheTransport) {
    int                          calls = 0;
    std::vector<qp::MarketEvent> events{
        make_funding(1, 1000, 0.0),
        make_funding(1, 2000, 0.0),
        make_funding(1, 3000, 0.0),
    };
    Portfolio portfolio{make_subscription()};
    CountTest engine{
        FakeTransport{events},
        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, CountingStrategy{&calls}, portfolio};

    while (engine.step()) {
    }
    EXPECT_EQ(calls, 3);
}

TEST(Engine, RunProcessesEveryEventThenExitsOnControlStop) {
    int                          calls = 0;
    std::vector<qp::MarketEvent> events{
        make_funding(1, 1000, 0.0),
        make_funding(1, 2000, 0.0),
        make_funding(1, 3000, 0.0),
    };
    Portfolio portfolio{make_subscription()};
    CountTest engine{
        FakeTransport{events},
        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, CountingStrategy{&calls}, portfolio};

    qp::ControlChannel<1> control;
    std::size_t           consumer = control.attach();
    std::thread           runner([&] { engine.run(control, consumer); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    control.request_stop();
    runner.join();

    EXPECT_EQ(calls, 3);
}

TEST(Engine, FundingEventSettlesAgainstCurrentPositionBeforeStrategyReacts) {
    std::vector<qp::MarketEvent> events{
        make_trade(1, 1000, 100.0),
        make_mark_price_kline(1, 1500, 100.0),
        make_funding(1, 2000, 0.0001),
    };
    Portfolio  portfolio{make_subscription()};
    SingleTest engine{
        FakeTransport{events},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
        portfolio};

    EXPECT_TRUE(engine.step());
    EXPECT_TRUE(engine.step());
    EXPECT_TRUE(engine.step());
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0 - 0.08 - 0.02);
}

TEST(Engine, SiblingEnginesShareOnePortfolioAcrossBothStrategiesFills) {
    std::vector<qp::MarketEvent> events_a{make_trade(1, 1000, 100.0)};
    std::vector<qp::MarketEvent> events_b{make_trade(1, 1000, 100.0)};
    Portfolio                    portfolio{make_subscription()};
    SingleTest                   engine_a{
        FakeTransport{events_a},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, SingleIntentStrategy{.symbol = 1, .target_position = 2.0},
        portfolio};
    SingleTest engine_b{
        FakeTransport{events_b},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysApproveRiskGate{}, SingleIntentStrategy{.symbol = 1, .target_position = 3.0},
        portfolio};

    EXPECT_TRUE(engine_a.step());
    EXPECT_TRUE(engine_b.step());

    EXPECT_EQ(portfolio.position(kExchange, kMarket, 1), 5.0);
}

TEST(Engine, RiskGateOnTickOrdersAreSubmittedAndAppliedToThePortfolio) {
    std::vector<qp::MarketEvent> events{make_trade(1, 1000, 100.0)};
    Portfolio                    portfolio{make_subscription()};
    qp::engine::Engine<FakeTransport, TestExec, AlwaysFlattenRiskGate, NoopStrategy> engine{
        FakeTransport{events},

        TestExec{portfolio, qp::execution::sim::matcher::last_trade::LastTradeMatcher{portfolio}},
        AlwaysFlattenRiskGate{.order = qp::Order{.id       = 1,
                                                 .exchange = kExchange,
                                                 .market   = kMarket,
                                                 .symbol   = 1,
                                                 .side     = qp::Side::Sell,
                                                 .qty      = 2.0}},
        NoopStrategy{}, portfolio};

    EXPECT_TRUE(engine.step());

    EXPECT_EQ(portfolio.position(kExchange, kMarket, 1), -2.0);
}
