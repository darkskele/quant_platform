#pragma once
#include <span>

#include "types.hpp"

namespace qp::execution {

/// The prod/test switch for order execution (mirrors Source/Sink). Fills and
/// rejects are separate streams, not one interleaved poll: every Fill/Reject
/// already carries its own order_id/ts, so nothing downstream needs arrival
/// order across the two kinds to know what happened to a given order — a
/// merged single channel would buy nothing this doesn't already give for
/// free.
template <class T>
concept ExecutionGateway = requires(T e, const MarketEvent& ev, Order o, Timestamp ts) {
    { e.on_market_event(ev) } -> std::same_as<void>;
    { e.submit(o, ts) } -> std::same_as<void>;
    { e.fills() } -> std::same_as<std::span<const Fill>>;
    { e.rejects() } -> std::same_as<std::span<const Reject>>;
};

}  // namespace qp::execution
