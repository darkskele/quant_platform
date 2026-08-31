#pragma once
#include <span>

#include "types.hpp"

namespace qp::execution {

/// Execution gateway concept.
template <class T>
concept ExecutionGateway = requires(T e, const MarketEvent& ev, Order o, Timestamp ts) {
    { e.on_market_event(ev) } -> std::same_as<void>;
    { e.submit(o, ts) } -> std::same_as<void>;
    { e.fills() } -> std::same_as<std::span<const Fill>>;
    { e.rejects() } -> std::same_as<std::span<const Reject>>;
};

}  // namespace qp::execution
