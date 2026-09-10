#pragma once
#include <optional>

#include "transport.hpp"
#include "types.hpp"

namespace qp::test {

/// Never exhausts — returns the same event every call. For per-step()
/// cost benchmarks, isolated from transport-exhaustion bookkeeping.
struct InfiniteTransport {
    MarketEvent event;

    std::optional<engine::transport::EngineInput> next() {
        return engine::transport::EngineInput{header_of(event).ts, event};
    }

    void flush() noexcept {}
};

/// Never exhausts.
struct SeedThenSteadyStateTransport {
    MarketEvent seed;
    MarketEvent steady_state;
    bool        seeded = false;

    std::optional<engine::transport::EngineInput> next() {
        const MarketEvent& e = seeded ? steady_state : seed;
        seeded               = true;
        return engine::transport::EngineInput{header_of(e).ts, e};
    }

    void flush() noexcept {}
};

}  // namespace qp::test
