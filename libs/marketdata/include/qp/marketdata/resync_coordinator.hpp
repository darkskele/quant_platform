#pragma once
#include <array>
#include <cassert>
#include <optional>
#include <vector>

#include "qp/core/types.hpp"
#include "qp/marketdata/gap_detector.hpp"
#include "qp/marketdata/resync.hpp"
#include "qp/venue/binance.hpp"

namespace qp {

// The per-symbol buffering/resync decision logic that used to live directly
// inside LiveWebSocketSource (buffer_event/begin_resync/complete_resync),
// pulled out for the same reason find_resync_point was: this is pure state,
// no I/O, no threading, so it can be driven directly and proven correct —
// including the retry/gap/hole paths that are genuinely hard to hit
// deterministically through real sockets and real thread timing. Not
// thread-safe: one owner (the I/O thread) only, same as SequenceGapDetector.
//
// This class does NOT do any I/O itself — it never fetches a snapshot and
// never touches a SymbolTable or a socket. It only decides: forward this
// event now, hold it, or hold it and tell the caller a snapshot request is
// needed. The caller (LiveWebSocketSource) is what actually dispatches
// requests and moves bytes.
class ResyncCoordinator {
public:
    enum class Action { Forward, Buffer, BufferAndRequest };

    struct Verdict {
        Action                      action;
        SymbolId                    symbol;
        std::optional<MarketEvent>  event;         // set only when action == Forward
        bool                        gap_detected = false;  // for the caller's own metrics
    };

    struct SnapshotOutcome {
        bool                       need_retry = false;   // true: no alignment found, caller must re-request
        std::vector<MarketEvent>   to_replay;            // events to forward, in order (empty if need_retry)
        std::size_t                internal_gaps = 0;    // discontinuities found *within* the replayed
                                                            // buffer — shouldn't happen (TCP is ordered),
                                                            // but counted rather than assumed
    };

    // One incoming BookDiff event. Caller is expected to have already
    // validated `event.symbol < kMaxSymbols` (this trusts it — DoS-by-
    // out-of-range-index isn't its job to defend against, that's a
    // structural invariant SymbolTable/the caller already own).
    Verdict on_event(MarketEvent event) {
        const SymbolId symbol = event.symbol;
        assert(symbol < kMaxSymbols);

        if (state_[symbol] == SymbolState::Buffering) {
            bool needs_request = buffer(symbol, std::move(event));
            return {needs_request ? Action::BufferAndRequest : Action::Buffer, symbol, std::nullopt, false};
        }

        if (gap_detector_.check_and_record(symbol, event.prev_seq, event.seq)) {
            buffer(symbol, std::move(event));  // buffer was empty (Streaming until now) -> always requests
            return {Action::BufferAndRequest, symbol, std::nullopt, true};
        }

        return {Action::Forward, symbol, std::move(event), false};
    }

    // A REST snapshot arrived for `symbol`. Either returns the events to
    // replay (buffer cleared, symbol transitions to Streaming, gap_detector
    // re-anchored to this snapshot rather than whatever it held before), or
    // signals a retry is needed (buffer cleared either way — see class docs
    // on why NOT clearing on a failed align isn't obviously better).
    SnapshotOutcome on_snapshot(SymbolId symbol, const venue::binance::DepthSnapshot& snapshot) {
        assert(symbol < kMaxSymbols);
        auto& buf = buffers_[symbol];
        auto  idx = find_resync_point(snapshot.last_update_id, buf);

        SnapshotOutcome outcome;
        if (!idx) {
            buf.clear();
            outcome.need_retry = true;
            return outcome;
        }

        // Re-anchor to the snapshot, not whatever gap_detector last held for
        // this symbol before the resync — the first replayed event has
        // nothing to compare against yet, same as SequenceGapDetector's own
        // "first event is never a gap" rule.
        gap_detector_.reset(symbol);
        outcome.to_replay.reserve(buf.size() - *idx);
        for (std::size_t i = *idx; i < buf.size(); ++i) {
            if (gap_detector_.check_and_record(symbol, buf[i].prev_seq, buf[i].seq)) {
                ++outcome.internal_gaps;
            }
            outcome.to_replay.push_back(std::move(buf[i]));
        }
        buf.clear();
        state_[symbol] = SymbolState::Streaming;
        return outcome;
    }

    static constexpr std::size_t kMaxSymbols          = SequenceGapDetector::kMaxSymbols;
    static constexpr std::size_t kBufferCapacity      = 256;  // generous vs. a sub-second REST round trip

private:
    enum class SymbolState { Buffering, Streaming };  // Buffering first: the default (unseen) state

    // Returns true if a (re)request must be issued: this push was the first
    // entry in a buffer that was empty (fresh Buffering, or just cleared by
    // an overflow retry).
    bool buffer(SymbolId symbol, MarketEvent event) {
        auto& buf       = buffers_[symbol];
        bool  was_empty = buf.empty();

        if (buf.size() >= kBufferCapacity) {
            buf.clear();
            was_empty = true;
        }

        buf.push_back(std::move(event));
        state_[symbol] = SymbolState::Buffering;
        return was_empty;
    }

    // Same reasoning as SequenceGapDetector: SymbolId is dense/small, direct
    // array indexing beats a hash map, and a mask would trade correctness
    // (aliasing two real symbols into one slot) for protection against a
    // problem — a large/sparse key space — that doesn't exist here.
    std::array<SymbolState, kMaxSymbols>              state_{};
    std::array<std::vector<MarketEvent>, kMaxSymbols> buffers_{};
    SequenceGapDetector                               gap_detector_;
};

}  // namespace qp
