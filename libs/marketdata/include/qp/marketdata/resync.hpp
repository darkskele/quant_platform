#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "qp/core/types.hpp"

namespace qp {

// Binance's documented alignment rule for resuming a diff stream from a REST
// snapshot: drop any buffered event with seq (u) < last_update_id (stale —
// already reflected in the snapshot), then the correct resume point is the
// first remaining event where first_seq (U) <= last_update_id+1 <= seq (u).
//
// Returns the index into `buffered` to replay from (that event and every one
// after it), or nullopt if no valid alignment point exists in what's
// buffered — the snapshot landed in a hole the buffer doesn't cover
// (shouldn't happen with a continuous buffer, but fail safe rather than
// guess), or every buffered event predates the snapshot. Either way the
// caller must discard the buffer and request a fresh snapshot.
inline std::optional<std::size_t> find_resync_point(std::uint64_t                    last_update_id,
                                                      const std::vector<MarketEvent>& buffered) {
    for (std::size_t i = 0; i < buffered.size(); ++i) {
        const auto& ev = buffered[i];
        if (ev.seq < last_update_id) continue;  // stale, already reflected in the snapshot
        if (ev.first_seq <= last_update_id + 1 && last_update_id + 1 <= ev.seq) return i;
        return std::nullopt;  // ev.seq is past the snapshot but doesn't bracket it — a hole
    }
    return std::nullopt;  // every buffered event predates the snapshot
}

}  // namespace qp
