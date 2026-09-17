#include <gtest/gtest.h>

#include "basic_risk_gate.hpp"
#include "exchange.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::risk::RiskOutcome;
using qp::risk::basic::BasicRiskGate;
using qp::risk::basic::BasicRiskGateConfig;
using qp::risk::basic::TrackedInstrument;
using qp::test::make_fill;
using qp::test::make_trade;

namespace {

constexpr SlotOffset kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kMarket   = 0;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    sub.add(ExchangeId::Binance, kMarket, "B");
    sub.add(ExchangeId::Binance, kMarket, "C");
    return std::move(sub).build();
}

qp::Intent intent(SlotOffset symbol, qp::Qty target) {
    return qp::Intent{
        .exchange = kExchange, .market = kMarket, .symbol = symbol, .target_position = target};
}

TrackedInstrument tracked(SlotOffset symbol) {
    return TrackedInstrument{.exchange = kExchange, .market = kMarket, .symbol = symbol};
}

using Portfolio = qp::Portfolio;

}  // namespace

TEST(BasicRiskGate, ApprovesAndSizesTheFullTargetWhenFlat) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(intent(1, 2.0));

    EXPECT_EQ(decision.outcome, RiskOutcome::Approved);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 2.0);
}

TEST(BasicRiskGate, SizesOrderByDeltaAgainstCurrentPositionNotTheRawTarget) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 3.0));
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(intent(1, 5.0));

    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 2.0);
}

TEST(BasicRiskGate, NegativeDeltaProducesASellOrder) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 5.0));
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(intent(1, 1.0));

    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Sell);
    EXPECT_DOUBLE_EQ(decision.order->qty, 4.0);
}

TEST(BasicRiskGate, ClampsTargetToMaxPositionQtyAndReportsResized) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 5.0, .max_drawdown = 1000.0},
                       portfolio};

    auto decision = gate.check(intent(1, 8.0));

    EXPECT_EQ(decision.outcome, RiskOutcome::Resized);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 5.0);
}

TEST(BasicRiskGate, ClampsNegativeTargetToMaxPositionQtyAndReportsResized) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 5.0, .max_drawdown = 1000.0},
                       portfolio};

    auto decision = gate.check(intent(1, -8.0));

    EXPECT_EQ(decision.outcome, RiskOutcome::Resized);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Sell);
    EXPECT_DOUBLE_EQ(decision.order->qty, 5.0);
}

TEST(BasicRiskGate, OnTickReturnsNoOrdersWhenEquityHasNotDrawnDown) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{}, portfolio};

    EXPECT_TRUE(gate.on_tick().empty());
}

TEST(BasicRiskGate, OnTickFlattensTrackedPositionsOnceEquityDrawsDownPastTheThreshold) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{
                           .max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {tracked(1)}},
                       portfolio};

    gate.check(intent(1, 2.0));
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0));
    portfolio.apply_mark_price(make_trade(1, 0, 100.0));

    EXPECT_TRUE(gate.on_tick().empty());

    portfolio.apply_mark_price(make_trade(1, 0, 20.0));

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].symbol, 1u);
    EXPECT_EQ(orders[0].market, kMarket);
    EXPECT_EQ(orders[0].side, qp::Side::Sell);
    EXPECT_DOUBLE_EQ(orders[0].qty, 2.0);
}

TEST(BasicRiskGate, OnTickFlattensAShortPositionWithABuyOrder) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{
                           .max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {tracked(1)}},
                       portfolio};

    gate.check(intent(1, -2.0));
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, 100.0));
    portfolio.apply_mark_price(make_trade(1, 0, 100.0));

    EXPECT_TRUE(gate.on_tick().empty());

    portfolio.apply_mark_price(make_trade(1, 0, 180.0));

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(orders[0].qty, 2.0);
}

TEST(BasicRiskGate, OnTickSkipsTrackedInstrumentsWithNoPosition) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{
        BasicRiskGateConfig{
            .max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {tracked(1), tracked(2)}},
        portfolio};

    gate.check(intent(1, 2.0));
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0));
    portfolio.apply_mark_price(make_trade(1, 0, 20.0));

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].symbol, 1u);
}

TEST(BasicRiskGate, OnTickFillsTheOrderPoolToExactCapacityWithoutOverflow) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 10.0,
                                           .max_drawdown     = 50.0,
                                           .tracked = {tracked(0), tracked(1), tracked(2)}},
                       portfolio};
    ASSERT_EQ(portfolio.max_instruments(), 3u);

    gate.check(intent(0, 1.0));
    gate.check(intent(1, 1.0));
    gate.check(intent(2, 1.0));
    portfolio.apply_fill(make_fill(0, qp::Side::Buy, 1.0, 100.0));
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0, 100.0));
    portfolio.apply_fill(make_fill(2, qp::Side::Buy, 1.0, 100.0));
    portfolio.apply_mark_price(make_trade(0, 0, 100.0));
    portfolio.apply_mark_price(make_trade(1, 0, 100.0));
    portfolio.apply_mark_price(make_trade(2, 0, 100.0));

    EXPECT_TRUE(gate.on_tick().empty());

    portfolio.apply_mark_price(make_trade(0, 0, 20.0));
    portfolio.apply_mark_price(make_trade(1, 0, 20.0));
    portfolio.apply_mark_price(make_trade(2, 0, 20.0));

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 3u);
    for (const auto& order : orders) {
        EXPECT_EQ(order.side, qp::Side::Sell);
        EXPECT_DOUBLE_EQ(order.qty, 1.0);
    }
}

TEST(BasicRiskGate, OnTickReturnsNoFurtherOrdersOnceAlreadyTripped) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{
        BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 0.0, .tracked = {tracked(1)}},
        portfolio};
    gate.check(intent(1, 1.0));
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0, 100.0, 1, 0, 1.0));

    auto first = gate.on_tick();
    ASSERT_FALSE(first.empty());

    EXPECT_TRUE(gate.on_tick().empty());
}

TEST(BasicRiskGate, RejectsEveryCheckOnceTripped) {
    Portfolio     portfolio{make_subscription()};
    BasicRiskGate gate{BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 0.0},
                       portfolio};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0, 100.0, 1, 0, 1.0));
    gate.on_tick();

    auto decision = gate.check(intent(2, 1.0));

    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}
