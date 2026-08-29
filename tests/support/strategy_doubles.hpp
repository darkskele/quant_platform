#pragma once
#include <span>

#include "types.hpp"

namespace qp::test {

struct NoopStrategy {
    std::span<const Intent> on_event(const MarketEvent&) { return {}; }

    std::span<const Intent> on_timer(Timestamp) { return {}; }
};

/// Emits the same Intent every call — for exercising the full
/// intent -> risk -> submit path repeatedly (e.g. Engine benchmarks),
/// unlike a fires-once double.
struct AlwaysIntentStrategy {
    SymbolId symbol          = 1;
    Qty      target_position = 1.0;
    Intent   intent_{};

    std::span<const Intent> on_event(const MarketEvent&) {
        intent_ = Intent{.symbol = symbol, .target_position = target_position};
        return {&intent_, 1};
    }

    std::span<const Intent> on_timer(Timestamp) { return {}; }
};

}  // namespace qp::test
