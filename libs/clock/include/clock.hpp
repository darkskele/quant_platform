#pragma once
#include "types.hpp"

namespace qp {

/// The seam anything needing "now" satisfies — strategy/risk code must
/// never call the system clock directly (docs/architecture-principles.md's
/// determinism landmine). Compile-time policy, mirrors Source/Sink.
/// advance() is the Engine loop's hook to hand a just-pulled event's
/// timestamp to the clock — a real mutation for SimClock, a no-op for
/// WallClock (see wall_clock.hpp/sim_clock.hpp).
template <class T>
concept Clock = requires(T c, Timestamp ts) {
    { c.now() } -> std::same_as<Timestamp>;
    { c.advance(ts) } -> std::same_as<void>;
};

}  // namespace qp
