#pragma once
#include "types.hpp"

namespace qp {

/// Backtest Clock: "now" is whatever timestamp the replay loop last handed
/// it via advance() — never the real system clock. 
class SimClock {
   public:
    Timestamp now() const noexcept { return ts_; }

    void advance(Timestamp ts) noexcept { ts_ = ts; }

   private:
    Timestamp ts_{};
};

}  // namespace qp
