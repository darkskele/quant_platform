#pragma once
#include <optional>

#include "types.hpp"

namespace qp::engine::transport {

/// One pull from the transport. ts is now, replay time in backtest and
/// receive time live, always meaningful. A present event is market data for
/// on_event; an absent one is a timer tick for on_timer at ts.
struct EngineInput {
    Timestamp                  ts{};
    std::optional<MarketEvent> event{};
};

/// The seam Engine actually consumes: one EngineInput at a time, or nullopt
/// when nothing is ready. flush() switches next() out of its "wait for every
/// leg" mode into "drain what's buffered without waiting".
template <class T>
concept Transport = requires(T t) {
    { t.next() } -> std::same_as<std::optional<EngineInput>>;
    { t.flush() } -> std::same_as<void>;
};

}  // namespace qp::engine::transport
