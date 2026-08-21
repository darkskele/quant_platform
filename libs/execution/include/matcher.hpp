#pragma once
#include <variant>

#include "types.hpp"

namespace qp::execution {

/// The seam SimExecution's fill sophistication plugs into — bundles
/// market-state tracking and fill computation as one policy, not two
/// independently swappable concepts: whatever on_market_event()
/// accumulates is exactly and only what try_fill() needs, so splitting
/// them would just invent a leaky interface between tightly-coupled
/// halves. Named Matcher (the standard term for "decides how an order
/// meets the market"), not FillModel — implementations range from
/// LastTradeMatcher (today) to a future book-aware one.
template <class T>
concept Matcher = requires(T m, MarketEvent ev, Order o, Timestamp ts) {
    { m.on_market_event(ev) } -> std::same_as<void>;
    { m.try_fill(o, ts) } -> std::same_as<std::variant<Fill, Reject>>;
};

}  // namespace qp::execution
