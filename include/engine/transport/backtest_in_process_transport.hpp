#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "control_channel.hpp"
#include "types.hpp"

namespace qp::engine::transport {

/// Backtest's own Transport: merges a fixed set of rings in ascending
/// MarketEvent::ts order.
/// next() buffers one popped-but-unreturned element per ring (a lookahead
/// slot). Ties go to the lowest ring index.
/// A ring with nothing buffered yet means next() can't safely order
/// anything else against it.
template <typename Ring, std::size_t N, std::size_t NumControlConsumers>
class BacktestInProcessTransport {
    static_assert(N >= 1);

   public:
    /// rings[i] paired with consumers[i] — this Engine's own consumer
    /// index on that leg's ring, positionally matched. control_consumer is
    /// this Engine's own index on `control`, from ControlChannel::attach().
    BacktestInProcessTransport(std::array<Ring*, N> rings, std::array<std::size_t, N> consumers,
                               ControlChannel<NumControlConsumers>& control,
                               std::size_t                          control_consumer)
        : rings_(rings),
          consumers_(consumers),
          control_(&control),
          control_consumer_(control_consumer) {}

    std::optional<MarketEvent> next() {
        if (!stopped_) {
            if (control_->poll(control_consumer_) == ControlCommand::Stop) stopped_ = true;
        }

        // Single pass: fills each empty lookahead slot and tracks the
        // running earliest at the same time. Can't return early on 
        // any_missing while running.
        bool                       any_missing = false;
        std::optional<std::size_t> earliest;
        for (std::size_t i = 0; i < N; ++i) {
            if (!lookahead_[i]) {
                if (auto popped = rings_[i]->try_pop(consumers_[i]))
                    lookahead_[i] = std::move(*popped);
                else {
                    any_missing = true;
                    continue;
                }
            }
            if (!earliest || header_of(*lookahead_[i]).ts < header_of(*lookahead_[*earliest]).ts)
                earliest = i;
        }

        // Still running and some leg has nothing buffered: can't safely
        // pick a winner. Stopped: flush whatever's available instead.
        if (any_missing && !stopped_) return std::nullopt;
        if (!earliest) return std::nullopt;  // stopped and genuinely nothing left anywhere

        // Move, not copy: the slot gets reset right after regardless, so
        // moving skips copying bids/asks for free.
        MarketEvent out = std::move(*lookahead_[*earliest]);
        lookahead_[*earliest].reset();
        return out;
    }

    /// True once Stop has been observed and every ring/lookahead slot is
    /// genuinely drained.
    bool is_done() const noexcept {
        if (!stopped_) return false;
        for (std::size_t i = 0; i < N; ++i) {
            if (lookahead_[i]) return false;
        }
        return true;
    }

   private:
    std::array<Ring*, N>                      rings_;
    std::array<std::size_t, N>                consumers_;
    std::array<std::optional<MarketEvent>, N> lookahead_{};
    ControlChannel<NumControlConsumers>*      control_;
    std::size_t                               control_consumer_;
    bool                                      stopped_ = false;
};

}  // namespace qp::engine::transport
