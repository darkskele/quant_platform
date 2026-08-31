#pragma once
#include <concepts>
#include <cstdint>
#include <optional>
#include <span>

#include "types.hpp"

namespace qp::risk {

/// Approved and Resized carry the same Order shape, only Rejected differs.
/// This says why the intent was decided, not what shape the result takes.
enum class RiskOutcome : std::uint8_t { Approved, Resized, Rejected };

struct RiskDecision {
    RiskOutcome          outcome{};
    std::optional<Order> order{};  ///< Absent when outcome is Rejected.
};

/// Turns an Intent into sized Order(s) and has final say over what actually
/// gets sent to a venue.
template <class T>
concept RiskGate = requires(T r, Intent intent) {
    /// Decision for this intent, sized or rejected against risk limits.
    { r.check(intent) } -> std::same_as<RiskDecision>;
    /// Orders produced by this tick, if risk limits require action.
    { r.on_tick() } -> std::same_as<std::span<const Order>>;
};

}  // namespace qp::risk
