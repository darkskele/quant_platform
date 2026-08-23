#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "resync_policy.hpp"
#include "types.hpp"

namespace qp::source {

// Finds where to resume a buffered diff stream against a REST snapshot's
// last_update_id. Drops any buffered event with seq (u) < last_update_id
// (stale — already reflected in the snapshot); the resume point is the
// first remaining event that `Rule` says brackets last_update_id — the
// exact comparison differs by market (see resync_policy.hpp, D13), so it's
// a template parameter, not inlined here.
//
// Returns the index into `buffered` to replay from (that event and every one
// after it), or nullopt if no valid alignment point exists in what's
// buffered — the snapshot landed in a hole the buffer doesn't cover
// (shouldn't happen with a continuous buffer, but fail safe rather than
// guess), or every buffered event predates the snapshot. Either way the
// caller must discard the buffer and request a fresh snapshot.
template <AlignmentRule Rule>
inline std::optional<std::size_t> find_resync_point(std::uint64_t                   last_update_id,
                                                    const std::vector<MarketEvent>& buffered) {
    for (std::size_t i = 0; i < buffered.size(); ++i) {
        const auto& ev = buffered[i];
        if (ev.seq < last_update_id) continue;  // stale, already reflected in the snapshot
        if (Rule::brackets(ev.first_seq, last_update_id, ev.seq)) return i;
        return std::nullopt;  // ev.seq is past the snapshot but doesn't bracket it — a hole
    }
    return std::nullopt;  // every buffered event predates the snapshot
}

}  // namespace qp::source
