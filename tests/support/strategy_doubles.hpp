#pragma once
#include <vector>

#include "portfolio.hpp"
#include "types.hpp"

namespace qp::test {

struct NoopStrategy {
    std::vector<Intent> on_event(const MarketEvent&, StateView) { return {}; }

    std::vector<Intent> on_timer(Timestamp, StateView) { return {}; }
};

/// Emits the same Intent every call — for exercising the full
/// intent -> risk -> submit path repeatedly (e.g. Engine benchmarks),
/// unlike a fires-once double.
struct AlwaysIntentStrategy {
    SymbolId symbol          = 1;
    Qty      target_position = 1.0;

    std::vector<Intent> on_event(const MarketEvent&, StateView) {
        return {Intent{.symbol = symbol, .target_position = target_position}};
    }

    std::vector<Intent> on_timer(Timestamp, StateView) { return {}; }
};

}  // namespace qp::test
