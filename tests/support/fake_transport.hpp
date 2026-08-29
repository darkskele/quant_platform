#pragma once
#include <optional>

#include "types.hpp"

namespace qp::test {

/// Never exhausts — returns the same event every call. For per-step()
/// cost benchmarks, isolated from transport-exhaustion bookkeeping.
struct InfiniteTransport {
    MarketEvent event;

    std::optional<MarketEvent> next() { return event; }
};

/// Never exhausts — returns `seed` once, then `steady_state` forever
/// after. For benchmarking the shape FundingCarryStrategy actually runs
/// in production: a Trade seeds SimExecution's matcher with a price once
/// (so submit() can fill, not just reject), then every subsequent step is
/// the Funding event a funding-reactive strategy actually reacts to —
/// apply_funding() + intent -> risk -> submit -> fill -> drain_outcomes,
/// all exercised together, not just the Trade-driven shape
/// InfiniteTransport alone gives you.
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
};

}  // namespace qp::test
