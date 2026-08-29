#include <gtest/gtest.h>

#include "basic_risk_gate.hpp"
#include "portfolio.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::risk::RiskOutcome;
using qp::risk::basic::BasicRiskGate;
using qp::risk::basic::BasicRiskGateConfig;
using qp::test::make_fill;
using qp::test::make_trade;

TEST(BasicRiskGate, ApprovesAndSizesTheFullTargetWhenFlat) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 2.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Approved);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 2.0);
}

TEST(BasicRiskGate, SizesOrderByDeltaAgainstCurrentPositionNotTheRawTarget) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 3.0));  // already long 3
    BasicRiskGate<qp::Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 5.0});

    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 2.0);  // 5 - 3
}

TEST(BasicRiskGate, NegativeDeltaProducesASellOrder) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 5.0));
    BasicRiskGate<qp::Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 1.0});

    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Sell);
    EXPECT_DOUBLE_EQ(decision.order->qty, 4.0);
}

TEST(BasicRiskGate, ClampsTargetToMaxPositionQtyAndReportsResized) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 5.0, .max_drawdown = 1000.0}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 8.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Resized);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_DOUBLE_EQ(decision.order->qty, 5.0);
}

TEST(BasicRiskGate, OnTickReturnsNoOrdersWhenEquityHasNotDrawnDown) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    EXPECT_TRUE(gate.on_tick().empty());
}

TEST(BasicRiskGate, OnTickFlattensTrackedPositionsOnceEquityDrawsDownPastTheThreshold) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {{1, 0}}},
        portfolio};

    // check() and the resulting fill are independent steps in the real
    // pipeline (RiskGate -> ExecutionGateway -> Portfolio::apply_fill).
    gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 2.0});
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/100.0));  // equity == 0 here

    EXPECT_TRUE(gate.on_tick().empty());  // establishes peak_equity_ == 0

    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/20.0));  // equity == -160: a 160 decline

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].symbol, 1u);
    EXPECT_EQ(orders[0].venue, 0);
    EXPECT_EQ(orders[0].side, qp::Side::Sell);  // flattens the long position
    EXPECT_DOUBLE_EQ(orders[0].qty, 2.0);
}

TEST(BasicRiskGate, OnTickReturnsNoFurtherOrdersOnceAlreadyTripped) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 0.0, .tracked = {{1, 0}}},
        portfolio};
    gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 1.0});
    portfolio.apply_fill(
        make_fill(1, qp::Side::Buy, 1.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0, /*fee=*/1.0));

    auto first = gate.on_tick();
    ASSERT_FALSE(first.empty());  // trips and flattens the tracked (1, 0) position

    EXPECT_TRUE(gate.on_tick().empty());
}

TEST(BasicRiskGate, RejectsEveryCheckOnceTripped) {
    qp::Portfolio                portfolio;
    BasicRiskGate<qp::Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 0.0}, portfolio};
    portfolio.apply_fill(
        make_fill(1, qp::Side::Buy, 1.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0, /*fee=*/1.0));
    gate.on_tick();  // trips immediately: any decline clears a 0.0 threshold

    auto decision = gate.check(qp::Intent{.symbol = 2, .venue = 0, .target_position = 1.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}
