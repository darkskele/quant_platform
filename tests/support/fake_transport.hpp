#pragma once
#include <optional>

#include "transport.hpp"
#include "types.hpp"

namespace qp::test {

struct InfiniteTransport {
    MarketEvent event;

    std::optional<engine::transport::EngineInput> next() {
        return engine::transport::EngineInput{event.base.ts, event};
    }

    void flush() noexcept {}
};

struct SeedThenSteadyStateTransport {
    MarketEvent seed;
    MarketEvent steady_state;
    bool        seeded = false;

    std::optional<engine::transport::EngineInput> next() {
        const MarketEvent& e = seeded ? steady_state : seed;
        seeded               = true;
        return engine::transport::EngineInput{e.base.ts, e};
    }

    void flush() noexcept {}
};

}  // namespace qp::test
