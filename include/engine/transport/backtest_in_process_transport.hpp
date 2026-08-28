#pragma once
#include <array>
#include <cstddef>
#include <optional>

#include "control_channel.hpp"
#include "types.hpp"

namespace qp::transport {

/// Backtest's own Transport policy: merges a fixed set of rings — each a
/// leg's fan-out ring, at whichever consumer index this Engine was handed
/// by that leg's FanoutSink::attach() (D43: indices can differ leg-to-leg,
/// since each FanoutSink's attach() counter is independent) — in ascending
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
    /// index on that leg's ring (from FanoutSink::attach()), positionally
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
            // pump() before poll(): request_stop() alone only lands in
            // ControlChannel's request inbox — nothing broadcasts it to
            // this consumer's ring until something calls pump(). This is
            // "whichever loop is already polling the channel" (its own
            // doc comment), so it does its own pumping rather than relying
            // on the composition root to remember to.
            control_->pump();
            if (control_->poll(control_consumer_) == ControlCommand::Stop) stopped_ = true;
        }

        bool any_missing = false;
        for (std::size_t i = 0; i < N; ++i) {
            if (!lookahead_[i]) {
                if (auto ptr = rings_[i]->try_pop(consumers_[i]))
                    lookahead_[i] = **ptr;
                else
                    any_missing = true;
            }
        }

        // Still running and some leg has nothing buffered: can't safely
        // pick a winner without risking a later, smaller timestamp from
        // that leg. Stopped: flush whatever's available instead.
        if (any_missing && !stopped_) return std::nullopt;

        std::optional<std::size_t> earliest;
        for (std::size_t i = 0; i < N; ++i) {
            if (!lookahead_[i]) continue;
            if (!earliest || lookahead_[i]->ts < lookahead_[*earliest]->ts) earliest = i;
        }
        if (!earliest) return std::nullopt;  // stopped and genuinely nothing left anywhere

        MarketEvent out = *lookahead_[*earliest];
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

}  // namespace qp::transport
