#pragma once
#include <concepts>
#include <cstddef>
#include <span>

#include "portfolio.hpp"
#include "types.hpp"
#include "viewable_pool.hpp"

namespace qp::strategy {

template <std::size_t N>
using IntentBuffer = ViewablePool<Intent, N>;

/// Emits intent, never a venue call (seam 4) — depends only on
/// MarketEvent/StateView/Intent. Static dispatch (D27). The returned span
/// aliases the strategy's own scratch buffer — valid until that strategy's
/// next on_event/on_timer call, no longer.
template <class T>
concept Strategy = requires(T s, const MarketEvent& event, StateView state, Timestamp now) {
    { s.on_event(event, state) } -> std::same_as<std::span<const Intent>>;
    { s.on_timer(now, state) } -> std::same_as<std::span<const Intent>>;
};

}  // namespace qp::strategy
