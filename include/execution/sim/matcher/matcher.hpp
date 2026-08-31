#pragma once
#include <variant>

#include "types.hpp"

namespace qp::execution::sim::matcher {

/// The seam SimExecution's fill sophistication plugs int.
/// Sim-only: a real exchange does its own matching, live has
/// nothing that plugs in here.
template <class T>
concept Matcher = requires(T m, const MarketEvent& ev, Order o, Timestamp ts) {
    { m.on_market_event(ev) } -> std::same_as<void>;
    { m.try_fill(o, ts) } -> std::same_as<std::variant<Fill, Reject>>;
};

}  // namespace qp::execution::sim::matcher
