#pragma once
#include <concepts>
#include <cstddef>
#include <span>

#include "types.hpp"
#include "viewable_pool.hpp"

namespace qp::strategy {

template <std::size_t N>
using IntentBuffer = ViewablePool<Intent, N>;

/// Emits intent, never a venue call (seam 4) — depends only on
/// MarketEvent/Intent plus whatever account state it holds its own
/// reference to. Static dispatch (D27). No Book/state parameter: a
/// concrete Strategy that needs live account state holds its own
/// PortfolioLike const Book& (same object Engine holds, wired at
/// construction by the composition root) — const, not just convention,
/// since a Strategy must never write to Portfolio. The returned span
/// aliases the strategy's own scratch buffer — valid until that
/// strategy's next on_event/on_timer call, no longer.
template <class T>
concept Strategy = requires(T s, const MarketEvent& event, Timestamp now) {
    { s.on_event(event) } -> std::same_as<std::span<const Intent>>;
    { s.on_timer(now) } -> std::same_as<std::span<const Intent>>;
};

}  // namespace qp::strategy
