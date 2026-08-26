#pragma once
#include <cmath>
#include <optional>
#include <vector>

#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "types.hpp"

namespace qp::test {

struct AlwaysApproveRiskGate {
    OrderId next_id{1};

    risk::RiskDecision check(Intent intent, StateView) {
        Side side = intent.target_position >= 0 ? Side::Buy : Side::Sell;
        return {.outcome = risk::RiskOutcome::Approved,
                .order   = Order{.id     = next_id++,
                                 .symbol = intent.symbol,
                                 .side   = side,
                                 .venue  = intent.venue,
                                 .qty    = std::abs(intent.target_position)}};
    }

    std::vector<Order> on_tick(StateView) { return {}; }
};

struct AlwaysRejectRiskGate {
    risk::RiskDecision check(Intent, StateView) {
        return {.outcome = risk::RiskOutcome::Rejected, .order = std::nullopt};
    }

    std::vector<Order> on_tick(StateView) { return {}; }
};

}  // namespace qp::test
