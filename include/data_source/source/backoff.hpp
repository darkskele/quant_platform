#pragma once
#include <algorithm>
#include <chrono>

namespace qp::source {

// Exponential backoff with a cap, for reconnect loops. next() returns the
// delay to use *now* and advances (doubles, capped) for the following call;
// reset() drops back to the initial delay after a successful connection.
// Pure — no sleeping, no I/O; the caller does the actual wait.
class ExponentialBackoff {
   public:
    explicit ExponentialBackoff(std::chrono::milliseconds initial = std::chrono::milliseconds{500},
                                std::chrono::milliseconds max = std::chrono::milliseconds{30000})
        : initial_(initial), max_(max), current_(initial) {}

    std::chrono::milliseconds next() {
        auto delay = current_;
        current_   = std::min(current_ * 2, max_);
        return delay;
    }

    void reset() { current_ = initial_; }

   private:
    std::chrono::milliseconds initial_;
    std::chrono::milliseconds max_;
    std::chrono::milliseconds current_;
};

}  // namespace qp::source
