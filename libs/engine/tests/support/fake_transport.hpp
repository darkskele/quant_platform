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

}  // namespace qp::test
