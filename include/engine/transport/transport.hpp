#pragma once
#include <optional>

#include "types.hpp"

namespace qp::engine::transport {

/// The seam Engine actually consumes — one MarketEvent at a time, or
/// nullopt. Structurally identical to source::Source (D22: the two cities
/// keep their own names) — anything satisfying one satisfies the other,
/// no relationship declared in code, concepts are structural. Distinct
/// name because Engine depends on this seam, not on "source" specifically
/// (a Transport can be a live/replay Source directly, or a
/// BacktestInProcessTransport merging several fan-out rings in timestamp
/// order).
template <class T>
concept Transport = requires(T t) {
    { t.next() } -> std::same_as<std::optional<MarketEvent>>;
};

}  // namespace qp::engine::transport
