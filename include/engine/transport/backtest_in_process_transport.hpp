#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "control_channel.hpp"
#include "types.hpp"

namespace qp::engine::transport {

/// Backtest's own Transport policy: merges a fixed set of rings — each a
/// leg's fan-out ring, at whichever consumer index this Engine was
/// assigned by the composition root (qp::venue_consumer_index, D43:
/// indices can differ leg-to-leg, since each leg's subscriber count is
/// independent) — in ascending
/// MarketEvent::ts order, not round-robin. Exchange-provided event
/// timestamps are comparable across venues on the same exchange (verified
/// against binance.cpp: futures and spot both stamp MarketEvent::ts from
/// Binance's own server-side event/trade time, not local receipt time) —
/// so this gives a deterministic merge order that depends only on the
/// data, never on producer-thread scheduling, which a scheduling-driven
/// round-robin merge can't guarantee once each
/// leg runs on its own thread (run_data_source).
///
/// next() buffers one popped-but-unreturned element per ring (a lookahead
/// slot) rather than peeking the ring directly — SpmcRing has no
/// non-consuming peek, and adding one would cost every consumer a second
/// read on the actual hot MarketEvent path for a need only this backtest-
/// only merge has. Ties (equal timestamps) go to the lowest ring index,
/// for a total order that doesn't depend on iteration happenstance.
///
/// While running, a ring with nothing buffered yet means next() can't
/// safely order anything else against it either — it returns nullopt
/// rather than guess. That's genuinely ambiguous (empty right now vs.
/// permanently done) without an explicit signal, so this holds a
/// ControlChannel reference (attached at construction, same convention as
/// the rings themselves) and polls it every call: once it observes Stop,
/// it latches stopped_ and switches to flush mode — emit the minimum
/// among whatever's actually buffered, no longer waiting on rings that
/// will never produce again, only returning nullopt once every ring and
/// every lookahead slot is genuinely empty.
template <typename Ring, std::size_t N, std::size_t NumControlConsumers>
class BacktestInProcessTransport {
    static_assert(N >= 1);

   public:
    /// rings[i] paired with consumers[i] — this Engine's own consumer
    /// index on that leg's ring (from qp::venue_consumer_index), positionally
    /// matched, same convention as run_data_source's source/sink pairing.
    /// control_consumer is this Engine's own index on `control`, from
    /// ControlChannel::attach().
    BacktestInProcessTransport(std::array<Ring*, N> rings, std::array<std::size_t, N> consumers,
                               ControlChannel<NumControlConsumers>& control,
                               std::size_t                          control_consumer)
        : rings_(rings),
          consumers_(consumers),
          control_(&control),
          control_consumer_(control_consumer) {}

    std::optional<MarketEvent> next() {
        if (!stopped_) {
            // No pump() here: ControlChannel now pumps itself on its own
            // thread (D5x) — several Engines each owning their own
            // Transport would otherwise be several concurrent pump()
            // callers racing on the same single-consumer inbox.
            if (control_->poll(control_consumer_) == ControlCommand::Stop) stopped_ = true;
        }

        // Single pass: fills each empty lookahead slot and tracks the
        // running earliest at the same time, instead of a fill pass
        // followed by a separate earliest-scan — each slot gets touched
        // once per call, not twice. Still can't return early on
        // any_missing while running: a still-missing ring might yet
        // produce a smaller timestamp than whatever's earliest so far, so
        // earliest can only be trusted once the whole pass has completed
        // (or once stopped_, where "missing" means "permanently gone").
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
        // pick a winner without risking a later, smaller timestamp from
        // that leg. Stopped: flush whatever's available instead.
        if (any_missing && !stopped_) return std::nullopt;
        if (!earliest) return std::nullopt;  // stopped and genuinely nothing left anywhere

        // Move, not copy: the slot gets reset right after regardless, so
        // there's nothing left to preserve in it — moving skips copying
        // bids/asks (real cost for a populated BookDiff) for free.
        MarketEvent out = std::move(*lookahead_[*earliest]);
        lookahead_[*earliest].reset();
        return out;
    }

    /// True once Stop has been observed and every ring/lookahead slot is
    /// genuinely drained — the caller's own driving loop needs this to
    /// tell "next() returned nullopt because it's mid-run and waiting" from
    /// "next() returned nullopt because there's truly nothing left", since
    /// Engine::run()'s while(step()){} can't make that distinction itself.
    /// Reflects state as of the last next() call, not a fresh ring check —
    /// call next() first, same way every other read of this class works.
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
