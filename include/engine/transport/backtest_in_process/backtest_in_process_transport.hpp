#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "spmc_queue.hpp"
#include "transport.hpp"
#include "types.hpp"

namespace qp::engine::transport {

/// Backtest's own Transport: merges a fixed set of queues in ascending
/// MarketEvent::ts order, and interleaves timer ticks on a fixed period in
/// the same replay-time line. next() buffers one popped-but-unreturned
/// element per queue (a lookahead slot).
template <std::size_t Capacity, std::size_t N, std::size_t NumConsumersPerQueue = 1>
class BacktestInProcessTransport {
    static_assert(N >= 1);

   public:
    using Queue = qp::SpmcQueue<MarketEvent, Capacity, NumConsumersPerQueue, /*UseHeap=*/true>;

    /// queues[i] paired with consumers[i]: this Engine's own consumer index
    /// on that leg's queue, positionally matched. timer_period 0 disables
    /// ticks.
    BacktestInProcessTransport(std::array<Queue*, N> queues, std::array<std::size_t, N> consumers,
                               Timestamp timer_period = 0)
        : queues_(queues), consumers_(consumers), timer_period_(timer_period) {}

    /// Drain-without-waiting from now on.
    void flush() noexcept { flushing_ = true; }

    std::optional<EngineInput> next() {
        // Single pass: fills each empty lookahead slot and tracks the
        // running earliest at the same time.
        bool                       any_missing = false;
        std::optional<std::size_t> earliest;
        for (std::size_t i = 0; i < N; ++i) {
            if (!lookahead_[i]) {
                if (auto popped = queues_[i]->try_pop(consumers_[i])) {
                    lookahead_[i] = std::move(*popped);
                } else {
                    // Flushing: an empty leg just contributes nothing.
                    if (!flushing_) any_missing = true;
                    continue;
                }
            }
            if (!earliest || header_of(*lookahead_[i]).ts < header_of(*lookahead_[*earliest]).ts)
                earliest = i;
        }

        if (any_missing) return std::nullopt;  // some leg might yet produce an earlier ts
        if (!earliest) return std::nullopt;    // nothing buffered anywhere

        Timestamp ev_ts = header_of(*lookahead_[*earliest]).ts;

        // A timer boundary at or before the next event fires first, at its
        // own ts, without consuming the event. Armed one period past the
        // first event seen.
        if (timer_period_ > 0) {
            if (!timer_armed_) {
                next_timer_  = ev_ts + timer_period_;
                timer_armed_ = true;
            }
            if (next_timer_ <= ev_ts) {
                Timestamp tick = next_timer_;
                next_timer_ += timer_period_;
                return EngineInput{.ts = tick, .event = std::nullopt};
            }
        }

        // Move, not copy: the slot gets reset right after regardless, so
        // moving skips copying bids/asks for free.
        MarketEvent out = std::move(*lookahead_[*earliest]);
        lookahead_[*earliest].reset();
        return EngineInput{.ts = ev_ts, .event = std::move(out)};
    }

   private:
    std::array<Queue*, N>                     queues_;
    std::array<std::size_t, N>                consumers_;
    std::array<std::optional<MarketEvent>, N> lookahead_{};
    Timestamp                                 timer_period_{0};
    Timestamp                                 next_timer_{0};
    bool                                      timer_armed_{false};
    bool                                      flushing_ = false;
};

}  // namespace qp::engine::transport
