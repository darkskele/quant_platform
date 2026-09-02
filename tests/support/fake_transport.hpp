#pragma once
#include <optional>

#include "types.hpp"

namespace qp::test {

/// Never exhausts — returns the same event every call. For per-step()
/// cost benchmarks, isolated from transport-exhaustion bookkeeping.
struct InfiniteTransport {
    MarketEvent event;

    std::optional<MarketEvent> next() { return event; }

    void flush() noexcept {}
};

/// Never exhausts.
struct SeedThenSteadyStateTransport {
    MarketEvent seed;
    MarketEvent steady_state;
    bool        seeded = false;

    std::optional<MarketEvent> next() {
        if (!seeded) {
            seeded = true;
            return seed;
        }
        return steady_state;
    }

    void flush() noexcept {}
};

}  // namespace qp::test
