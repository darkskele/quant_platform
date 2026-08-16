#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "types.hpp"

namespace qp::source {

// Binance's documented alignment rule for resuming a diff stream from a REST
// snapshot, USD-M FUTURES specifically: drop any buffered event with seq (u)
// < last_update_id (stale — already reflected in the snapshot), then the
// correct resume point is the first remaining event where
// first_seq (U) <= last_update_id <= seq (u).
//
// No +1 here — that's the SPOT market's convention (U <= lastUpdateId+1 <=
// u); this project is USD-M futures only (MISSION.md), documented as plain
// U <= lastUpdateId <= u, no offset. Confirmed against real captured
// snapshot+diff data during collector smoke-testing: the wrong (+1) rule
// systematically failed whenever last_update_id landed exactly on a
// buffered event's `seq` — a boundary real data hits often (Binance's
// snapshot generation correlates closely with the diff stream's own
// update-id sequencing) — because the +1 offset made that otherwise-valid
// bracketing event fail the check, discarding the buffer for a `nullopt`
// "hole" that was never really one. See docs/decisions.md D13.
//
// Returns the index into `buffered` to replay from (that event and every one
// after it), or nullopt if no valid alignment point exists in what's
// buffered — the snapshot landed in a hole the buffer doesn't cover
// (shouldn't happen with a continuous buffer, but fail safe rather than
// guess), or every buffered event predates the snapshot. Either way the
// caller must discard the buffer and request a fresh snapshot.
inline std::optional<std::size_t> find_resync_point(std::uint64_t                   last_update_id,
                                                    const std::vector<MarketEvent>& buffered) {
    for (std::size_t i = 0; i < buffered.size(); ++i) {
        const auto& ev = buffered[i];
        if (ev.seq < last_update_id) continue;  // stale, already reflected in the snapshot
        if (ev.first_seq <= last_update_id && last_update_id <= ev.seq) return i;
        return std::nullopt;  // ev.seq is past the snapshot but doesn't bracket it — a hole
    }
    return std::nullopt;  // every buffered event predates the snapshot
}

}  // namespace qp::source
