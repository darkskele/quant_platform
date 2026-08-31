#pragma once
#include <concepts>
#include <cstddef>
#include <span>

#include "types.hpp"
#include "viewable_pool.hpp"

namespace qp::strategy {

/// Scratch buffer a strategy uses to build its per-call Intent output.
template <std::size_t N>
using IntentBuffer = ViewablePool<Intent, N>;

/// Turns market events into target-position Intents.
/// Returned spans alias the strategy's own scratch buffer
/// and are valid only until its next on_event/on_timer call.
template <class T>
concept Strategy = requires(T s, const MarketEvent& event, Timestamp now) {
    /// Intents produced by this market event.
    { s.on_event(event) } -> std::same_as<std::span<const Intent>>;
    /// Intents produced by this timer tick.
    { s.on_timer(now) } -> std::same_as<std::span<const Intent>>;
};

}  // namespace qp::strategy
