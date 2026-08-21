#pragma once
#include "types.hpp"

namespace qp {

/// Backtest Clock: "now" is whatever timestamp the replay loop last handed
/// it via advance() — never the real system clock. Determinism landmine:
/// see docs/architecture-principles.md. No autonomous behavior — a slot the
/// Engine loop writes into once per pulled event; advance() doesn't
/// validate ordering, callers (a replay's own timestamp-merged stream) are
/// trusted to hand it non-decreasing values.
///
/// No benchmark: now()/advance() are a single scalar load/store each, no
/// algorithm to characterize, and inline away entirely at the Engine loop's
/// call site — a deliberate exemption (root docs/DESIGN.md goal 4), not an
/// oversight.
class SimClock {
   public:
    Timestamp now() const noexcept { return ts_; }

    void advance(Timestamp ts) noexcept { ts_ = ts; }

   private:
    Timestamp ts_{};
};

}  // namespace qp
