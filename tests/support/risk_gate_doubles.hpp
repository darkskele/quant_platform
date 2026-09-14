#pragma once
#include <cmath>
#include <optional>
#include <span>

#include "risk_gate.hpp"
#include "types.hpp"

namespace qp::test {

struct AlwaysApproveRiskGate {
    OrderId next_id{1};

    risk::RiskDecision check(Intent intent) {
        Side side = intent.target_position >= 0 ? Side::Buy : Side::Sell;
        return {.outcome = risk::RiskOutcome::Approved,
                .order   = Order{.id     = next_id++,
                                 .symbol = intent.symbol,
                                 .side   = side,
                                 .market  = intent.market,
                                 .qty    = std::abs(intent.target_position)}};
    }

    std::span<const Order> on_tick() { return {}; }
};

struct AlwaysRejectRiskGate {
    risk::RiskDecision check(Intent) {
        return {.outcome = risk::RiskOutcome::Rejected, .order = std::nullopt};
    }

    std::span<const Order> on_tick() { return {}; }
};

/// on_tick() fires `order` once, then goes quiet — for proving Engine
/// forwards a RiskGate's autonomous orders (no Intent involved) through
/// submit(), independent of the Intent -> check() path.
struct AlwaysFlattenRiskGate {
    Order order;
    bool  fired = false;

    risk::RiskDecision check(Intent) {
        return {.outcome = risk::RiskOutcome::Rejected, .order = std::nullopt};
    }

    std::span<const Order> on_tick() {
        if (fired) return {};
        fired = true;
        return {&order, 1};
    }
};

}  // namespace qp::test
