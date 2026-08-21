#include <gtest/gtest.h>

#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "support/risk_gate_doubles.hpp"
#include "types.hpp"

using qp::test::AlwaysApproveRiskGate;
using qp::test::AlwaysRejectRiskGate;

static_assert(qp::risk::RiskGate<AlwaysApproveRiskGate>);
static_assert(qp::risk::RiskGate<AlwaysRejectRiskGate>);

TEST(RiskGate, ApprovedDecisionCarriesAnOrderSizedFromIntent) {
    qp::Portfolio         portfolio;
    AlwaysApproveRiskGate gate;

    auto decision = gate.check(qp::Intent{.symbol = 1, .target_position = 2.0}, portfolio.view());

    EXPECT_EQ(decision.outcome, qp::risk::RiskOutcome::Approved);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->symbol, 1u);
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_EQ(decision.order->qty, 2.0);
}

TEST(RiskGate, RejectedDecisionCarriesNoOrder) {
    qp::Portfolio        portfolio;
    AlwaysRejectRiskGate gate;

    auto decision = gate.check(qp::Intent{.symbol = 1, .target_position = 2.0}, portfolio.view());

    EXPECT_EQ(decision.outcome, qp::risk::RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}
