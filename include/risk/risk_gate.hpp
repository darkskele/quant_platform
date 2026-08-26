#pragma once
#include <concepts>
#include <cstdint>
#include <optional>
#include <vector>

#include "portfolio.hpp"
#include "types.hpp"

namespace qp::risk {

/// Approved/Resized share one payload shape (an Order) — only Rejected
/// differs, so this tag says *why*, not *what shape* (unlike Fill/Reject,
/// which genuinely differ in fields).
enum class RiskOutcome : std::uint8_t { Approved, Resized, Rejected };

/// order is set if outcome != Rejected.
struct RiskDecision {
    RiskOutcome          outcome{};
    std::optional<Order> order{};
};

/// Where Intent becomes sized Order(s) — has its own authority, not a
/// pass-through (seam 5). on_tick() acts autonomously (kill-switch/
/// drawdown flatten), with no Intent input. Static dispatch (D27).
template <class T>
concept RiskGate = requires(T r, Intent intent, StateView state) {
    { r.check(intent, state) } -> std::same_as<RiskDecision>;
    { r.on_tick(state) } -> std::same_as<std::vector<Order>>;
};

}  // namespace qp::risk
