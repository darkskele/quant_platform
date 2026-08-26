#pragma once
#include <concepts>
#include <vector>

#include "portfolio.hpp"
#include "types.hpp"

namespace qp {

/// Emits intent, never a venue call (seam 4) — depends only on
/// MarketEvent/StateView/Intent. Static dispatch (D27).
template <class T>
concept Strategy = requires(T s, const MarketEvent& event, StateView state, Timestamp now) {
    { s.on_event(event, state) } -> std::same_as<std::vector<Intent>>;
    { s.on_timer(now, state) } -> std::same_as<std::vector<Intent>>;
};

}  // namespace qp
