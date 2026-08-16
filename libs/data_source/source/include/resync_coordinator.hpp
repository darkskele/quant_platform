#pragma once
#include <array>
#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "gap_detector.hpp"
#include "resync.hpp"
#include "types.hpp"

namespace qp::source {

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
        Action                     action;
        SymbolId                   symbol;
        std::optional<MarketEvent> event;                      // set only when action == Forward
        bool                       gap_detected      = false;  // for the caller's own metrics
        bool                       buffer_overflowed = false;  // BufferAndRequest caused by hitting
                                                               // kBufferCapacity, not a fresh/
                                                               // gap-triggered start — distinct
                                                               // failure mode, worth telling apart
                                                               // from a REST failure or a
                                                               // non-aligning snapshot
    };

    struct SnapshotOutcome {
        bool need_retry = false;             // true: no alignment found, caller must re-request
        std::vector<MarketEvent> to_replay;  // events to forward, in order (empty if need_retry)
        std::size_t              internal_gaps = 0;  // discontinuities found *within* the replayed
                                        // buffer — shouldn't happen (TCP is ordered),
                                        // but counted rather than assumed

        // Populated only when need_retry — not required for correctness,
        // just makes a future alignment failure self-diagnosing from a live
        // log instead of needing instrumentation re-derived from scratch
        // (D13 took exactly that to root-cause once already). Data only,
        // no I/O here — the caller decides whether/how to log it.
        std::string retry_detail;
    };

    // One incoming BookDiff event. Caller is expected to have already
    // validated `event.symbol < kMaxSymbols` (this trusts it — DoS-by-
    // out-of-range-index isn't its job to defend against, that's a
    // structural invariant SymbolTable/the caller already own).
    Verdict on_event(MarketEvent event) {
        const SymbolId symbol = event.symbol;
        assert(symbol < kMaxSymbols);

        if (state_[symbol] == SymbolState::Buffering) {
            auto result = buffer(symbol, std::move(event));
            return Verdict{
                .action       = result.needs_request ? Action::BufferAndRequest : Action::Buffer,
                .symbol       = symbol,
                .event        = std::nullopt,
                .gap_detected = false,
                .buffer_overflowed = result.was_overflow};
        }

        if (gap_detector_.check_and_record(symbol, event.prev_seq, event.seq)) {
            buffer(symbol,
                   std::move(event));  // buffer was empty (Streaming until now) -> always requests,
                                       // never an overflow (nothing was in it to overflow)
            return Verdict{.action            = Action::BufferAndRequest,
                           .symbol            = symbol,
                           .event             = std::nullopt,
                           .gap_detected      = true,
                           .buffer_overflowed = false};
        }

        return Verdict{.action            = Action::Forward,
                       .symbol            = symbol,
                       .event             = std::move(event),
                       .gap_detected      = false,
                       .buffer_overflowed = false};
    }

    // A REST snapshot arrived for `symbol` — last_update_id for alignment,
    // plus its bids/asks (generic PriceLevels, not a venue-specific type —
    // stays venue-agnostic per DESIGN.md G2) to forward as the anchor event.
    // Either returns the events to replay — the snapshot itself first, as a
    // BookSnapshot, then the buffered diffs from the alignment point
    // onward, now guaranteed to apply cleanly on top of it — with the
    // buffer cleared and symbol transitioned to Streaming, gap_detector
    // re-anchored rather than whatever it held before; or signals a retry
    // is needed. Buffer cleared either way on a failed align: keeping stale
    // events around wouldn't help the next snapshot align either (they
    // already failed to bracket this one), and holding them just grows the
    // buffer toward kBufferCapacity for no benefit.
    SnapshotOutcome on_snapshot(SymbolId symbol, std::uint64_t last_update_id,
                                std::vector<PriceLevel> bids = {},
                                std::vector<PriceLevel> asks = {}) {
        assert(symbol < kMaxSymbols);
        auto& buf = buffers_[symbol];
        auto  idx = find_resync_point(last_update_id, buf);

        SnapshotOutcome outcome;
        if (!idx) {
            outcome.retry_detail =
                buf.empty()
                    ? "no alignment: buffer empty"
                    : "no alignment: last_update_id=" + std::to_string(last_update_id) +
                          " buffered=[" + std::to_string(buf.front().first_seq) + ".." +
                          std::to_string(buf.back().seq) + "] size=" + std::to_string(buf.size());
            buf.clear();
            outcome.need_retry = true;
            return outcome;
        }

        // Re-anchor to the snapshot, not whatever gap_detector last held for
        // this symbol before the resync — the first replayed event has
        // nothing to compare against yet, same as SequenceGapDetector's own
        // "first event is never a gap" rule.
        gap_detector_.reset(symbol);

        // ts borrowed from the first replayed diff (guaranteed to exist —
        // `idx` was found, so buf[*idx] is there) rather than left at its
        // default (0): FileRecorder partitions by event.ts, and an
        // unset/zero timestamp would land the snapshot in a spurious
        // 1970-01-01 partition instead of alongside the diffs it anchors.
        MarketEvent snapshot_event;
        snapshot_event.kind   = EventKind::BookSnapshot;
        snapshot_event.symbol = symbol;
        snapshot_event.ts     = buf[*idx].ts;
        snapshot_event.seq    = last_update_id;  // the anchor point; first_seq/prev_seq unused —
                                                 // this isn't part of the diff-continuity chain
        snapshot_event.bids = std::move(bids);
        snapshot_event.asks = std::move(asks);

        outcome.to_replay.reserve(buf.size() - *idx + 1);
        outcome.to_replay.push_back(std::move(snapshot_event));
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

    static constexpr std::size_t kMaxSymbols = SequenceGapDetector::kMaxSymbols;
    static constexpr std::size_t kBufferCapacity =
        256;  // generous vs. a sub-second REST round trip

   private:
    enum class SymbolState { Buffering, Streaming };  // Buffering first: the default (unseen) state

    struct BufferResult {
        bool needs_request;  // this push was the first entry in a buffer that was empty
                             // (fresh Buffering, or just cleared by an overflow retry)
        bool was_overflow;   // specifically: needs_request because capacity was hit, not
                             // because the buffer was genuinely empty beforehand
    };

    BufferResult buffer(SymbolId symbol, MarketEvent event) {
        auto& buf       = buffers_[symbol];
        bool  was_empty = buf.empty();
        bool  overflow  = false;

        if (buf.size() >= kBufferCapacity) {
            buf.clear();
            was_empty = true;
            overflow  = true;
        }

        buf.push_back(std::move(event));
        state_[symbol] = SymbolState::Buffering;
        return {was_empty, overflow};
    }

    // Same reasoning as SequenceGapDetector: SymbolId is dense/small, direct
    // array indexing beats a hash map, and a mask would trade correctness
    // (aliasing two real symbols into one slot) for protection against a
    // problem — a large/sparse key space — that doesn't exist here.
    std::array<SymbolState, kMaxSymbols>              state_{};
    std::array<std::vector<MarketEvent>, kMaxSymbols> buffers_{};
    SequenceGapDetector                               gap_detector_;
};

}  // namespace qp::source
