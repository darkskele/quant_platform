#include <gtest/gtest.h>

#include <array>
#include <cstddef>

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

namespace {
constexpr std::array<std::size_t, 1> kCounts{3};
using Portfolio = qp::Portfolio<kCounts>;
}  // namespace

TEST(BasicRiskGate, ApprovesAndSizesTheFullTargetWhenFlat) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 2.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Approved);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 2.0);
}

TEST(BasicRiskGate, SizesOrderByDeltaAgainstCurrentPositionNotTheRawTarget) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 3.0));  // already long 3
    BasicRiskGate<Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 5.0});

    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 2.0);  // 5 - 3
}

TEST(BasicRiskGate, NegativeDeltaProducesASellOrder) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 5.0));
    BasicRiskGate<Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 1.0});

    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Sell);
    EXPECT_DOUBLE_EQ(decision.order->qty, 4.0);
}

TEST(BasicRiskGate, ClampsTargetToMaxPositionQtyAndReportsResized) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 5.0, .max_drawdown = 1000.0}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 8.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Resized);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_DOUBLE_EQ(decision.order->qty, 5.0);
}

TEST(BasicRiskGate, ClampsNegativeTargetToMaxPositionQtyAndReportsResized) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 5.0, .max_drawdown = 1000.0}, portfolio};

    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = -8.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Resized);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->side, qp::Side::Sell);
    EXPECT_DOUBLE_EQ(decision.order->qty, 5.0);
}

TEST(BasicRiskGate, OnTickReturnsNoOrdersWhenEquityHasNotDrawnDown) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{BasicRiskGateConfig{}, portfolio};

    EXPECT_TRUE(gate.on_tick().empty());
}

TEST(BasicRiskGate, OnTickFlattensTrackedPositionsOnceEquityDrawsDownPastTheThreshold) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
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

TEST(BasicRiskGate, OnTickFlattensAShortPositionWithABuyOrder) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {{1, 0}}},
        portfolio};

    gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = -2.0});
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/100.0));  // equity == 0 here

    EXPECT_TRUE(gate.on_tick().empty());  // establishes peak_equity_ == 0

    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/180.0));  // equity == -160: a 160 decline

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].side, qp::Side::Buy);  // flattens the short position
    EXPECT_DOUBLE_EQ(orders[0].qty, 2.0);
}

TEST(BasicRiskGate, OnTickSkipsTrackedInstrumentsWithNoPosition) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
        BasicRiskGateConfig{
            .max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {{1, 0}, {2, 0}}},
        portfolio};

    // Symbol 2 is tracked but never traded — on_tick() must not emit a
    // spurious order for a position that's already flat.
    gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 2.0});
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/20.0));  // decline big enough to trip

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].symbol, 1u);
}

// orders_ is a heap-backed ViewablePool<Order, Book::kMaxInstruments> — this
// drives on_tick() with exactly kMaxInstruments tracked, nonzero positions
// so it pushes to the pool's exact capacity in one call. Proves the pool
// doesn't overflow at the boundary, which push()'s own bounds check can't
// do for us in a release build (assert compiles out under NDEBUG).
TEST(BasicRiskGate, OnTickFillsTheOrderPoolToExactCapacityWithoutOverflow) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
        BasicRiskGateConfig{
            .max_position_qty = 10.0, .max_drawdown = 50.0, .tracked = {{0, 0}, {1, 0}, {2, 0}}},
        portfolio};
    static_assert(Portfolio::kMaxInstruments == 3);  // tracked above must match this exactly

    gate.check(qp::Intent{.symbol = 0, .venue = 0, .target_position = 1.0});
    gate.check(qp::Intent{.symbol = 1, .venue = 0, .target_position = 1.0});
    gate.check(qp::Intent{.symbol = 2, .venue = 0, .target_position = 1.0});
    portfolio.apply_fill(make_fill(0, qp::Side::Buy, 1.0, /*price=*/100.0));
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0, /*price=*/100.0));
    portfolio.apply_fill(make_fill(2, qp::Side::Buy, 1.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_trade(0, 0, /*price=*/100.0));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/100.0));
    portfolio.apply_mark_price(make_trade(2, 0, /*price=*/100.0));  // equity == 0 here

    EXPECT_TRUE(gate.on_tick().empty());  // establishes peak_equity_ == 0

    portfolio.apply_mark_price(make_trade(0, 0, /*price=*/20.0));
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/20.0));
    portfolio.apply_mark_price(make_trade(2, 0, /*price=*/20.0));  // 240 decline: trips

    auto orders = gate.on_tick();
    ASSERT_EQ(orders.size(), 3u);
    for (const auto& order : orders) {
        EXPECT_EQ(order.side, qp::Side::Sell);
        EXPECT_DOUBLE_EQ(order.qty, 1.0);
    }
}

TEST(BasicRiskGate, OnTickReturnsNoFurtherOrdersOnceAlreadyTripped) {
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
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
    Portfolio                portfolio;
    BasicRiskGate<Portfolio> gate{
        BasicRiskGateConfig{.max_position_qty = 10.0, .max_drawdown = 0.0}, portfolio};
    portfolio.apply_fill(
        make_fill(1, qp::Side::Buy, 1.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0, /*fee=*/1.0));
    gate.on_tick();  // trips immediately: any decline clears a 0.0 threshold

    auto decision = gate.check(qp::Intent{.symbol = 2, .venue = 0, .target_position = 1.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}
