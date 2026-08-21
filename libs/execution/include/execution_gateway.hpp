#pragma once
#include <optional>
#include <variant>

#include "types.hpp"

namespace qp::execution {

/// The prod/test switch for order execution (mirrors Source/Sink). Fills
/// and rejects come back through one ordered poll, not separate channels —
/// two independent queues would lose the true order outcomes happened in
/// (a fill, then a reject, then a fill again — draining one queue then the
/// other scrambles that).
template <class T>
concept ExecutionGateway = requires(T e, MarketEvent ev, Order o, Timestamp ts) {
    { e.on_market_event(ev) } -> std::same_as<void>;
    { e.submit(o, ts) } -> std::same_as<void>;
    { e.next_outcome() } -> std::same_as<std::optional<std::variant<Fill, Reject>>>;
};

}  // namespace qp::execution
