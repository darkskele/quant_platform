#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>

#include "types.hpp"

namespace qp::source {

// Details of a detected discontinuity, for logging.
struct GapInfo {
    std::uint64_t expected_prev_seq;
    std::uint64_t actual_prev_seq;
};

// Tracks the last final-update-id (seq) seen per symbol and flags when a new
// BookDiff event doesn't continue from it (event.prev_seq != last seq). Pure
// bookkeeping, no I/O — the caller decides what a gap means (log it, trigger
// a resync, etc.); this only detects. Not thread-safe: one owner (the I/O
// thread) only.
//
// `symbol` is a dense index into the venue's symbol table (SymbolTable::
// intern() hands out sequential 0, 1, 2...), so this is a fixed-capacity
// stack array, direct-indexed — no hashing, no heap, ever, not even a
// one-time allocation. kMaxSymbols is a generous cap for the number of
// full-depth (gap-tracked) target symbols one connection realistically
// subscribes to — docs/strategy.md's "a handful, not hundreds"; the
// broader/coarse universe doesn't get depth diffs, so never touches this.
//
// A modulo/mask scheme is deliberately not used: that trades correctness
// for boundedness on a large or sparse key space, which isn't our
// situation — our key space is already small and dense, so wraparound
// would only add aliasing risk between two different symbols for no
// benefit.
//
// The first event ever seen for a symbol is never a gap — there's nothing to
// compare it against yet.
class SequenceGapDetector {
   public:
    static constexpr std::size_t kMaxSymbols = 64;

    // Returns gap details if `prev_seq` doesn't continue from the last
    // recorded seq for this symbol; nullopt if continuous (or this is the
    // first event ever seen for the symbol). Always records `seq`
    // afterward, gap or not, so the next call continues from here.
    std::optional<GapInfo> check_and_record(SymbolId symbol, std::uint64_t prev_seq,
                                            std::uint64_t seq) {
        assert(symbol < kMaxSymbols &&
               "more symbols than SequenceGapDetector::kMaxSymbols — raise the cap");

        auto&                  last = last_seq_[symbol];
        std::optional<GapInfo> gap;
        // Branch hints match steady-state reality: after the first message
        // per symbol, "already tracked" is the near-certain case, and within
        // that, an actual discontinuity is the rare one — a healthy
        // connection hits neither branch's cold path essentially ever.
        if (last.has_value()) [[likely]] {
            if (prev_seq != *last) [[unlikely]] {
                gap = GapInfo{*last, prev_seq};
            }
        }
        last = seq;
        return gap;
    }

    // Forgets a symbol's state — its next event is treated as the first ever.
    void reset(SymbolId symbol) {
        assert(symbol < kMaxSymbols);
        last_seq_[symbol].reset();
    }

   private:
    std::array<std::optional<std::uint64_t>, kMaxSymbols> last_seq_{};
};

}  // namespace qp::source
