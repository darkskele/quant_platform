#pragma once
#include <span>

#include "types.hpp"

namespace qp::test {

struct NoopStrategy {
    std::span<const Intent> on_event(const MarketEvent&) { return {}; }

    std::span<const Intent> on_timer(Timestamp) { return {}; }
};

struct AlwaysIntentStrategy {
    std::uint16_t exchange        = 0;
    std::uint16_t market          = 0;
    std::uint16_t symbol          = 1;
    Qty           target_position = 1.0;
    Intent        intent_{};

    std::span<const Intent> on_event(const MarketEvent&) {
        intent_ = Intent{.exchange        = exchange,
                         .market          = market,
                         .symbol          = symbol,
                         .target_position = target_position};
        return {&intent_, 1};
    }

    std::span<const Intent> on_timer(Timestamp) { return {}; }
};

}  // namespace qp::test
