#pragma once
#include <optional>

#include "types.hpp"

namespace qp {

/// Backtest Clock: "now" is whatever timestamp the replay loop last handed
/// it via advance(), never the real system clock. With a non-zero
/// timer_period it also emits timer instants on that cadence in replay time.
class SimClock {
   public:
    explicit SimClock(Timestamp timer_period = 0) noexcept : timer_period_{timer_period} {}

    Timestamp now() const noexcept { return ts_; }

    void advance(Timestamp ts) noexcept { ts_ = ts; }

    /// Next timer instant at or before now(), consuming it, else nullopt.
    /// Armed one period past the first advance; disabled at period 0.
    std::optional<Timestamp> next_due_timer() noexcept {
        if (timer_period_ <= 0) return std::nullopt;
        if (!armed_) {
            next_timer_ = ts_ + timer_period_;
            armed_      = true;
        }
        if (next_timer_ > ts_) return std::nullopt;
        Timestamp due = next_timer_;
        next_timer_ += timer_period_;
        return due;
    }

   private:
    Timestamp ts_{};
    Timestamp timer_period_{0};
    Timestamp next_timer_{0};
    bool      armed_{false};
};

}  // namespace qp
