#pragma once
#include <array>
#include <cstddef>

#include "types.hpp"

namespace qp {

/// One strategy's subscription to one venue — an edge in the strategy <->
/// venue bipartite graph, known in full at compile time (every strategy
/// and every venue it needs is fixed by the composition root, never
/// discovered at runtime). Declaration order across the whole edge list
/// is the ordering venue_consumer_index() assigns ring slots from — this
/// is what replaces FanoutSink::attach()'s runtime fetch_add: every
/// consumer's ring slot becomes a compile-time constant instead of a race
/// to claim one (see data_source/sink/fanout/fanout_sink.hpp).
struct VenueSubscription {
    std::size_t strategy;  ///< Index into the composition root's strategy list.
    VenueId     venue;
};

/// Number of strategies subscribed to `venue` — the NumConsumers a
/// venue's FanoutSink<Capacity, NumConsumers> must be sized for.
template <std::size_t N>
consteval std::size_t venue_consumer_count(const std::array<VenueSubscription, N>& subs,
                                           VenueId                                 venue) {
    std::size_t count = 0;
    for (const auto& s : subs)
        if (s.venue == venue) ++count;
    return count;
}

/// `strategy`'s consumer index on `venue`'s ring: its rank (0-based) among
/// every subscription to `venue` that appears before its own entry in
/// `subs`. `consteval` (not just `constexpr`): every call is an immediate
/// invocation, evaluated during compilation with no runtime fallback, so
/// there's no way to accidentally pay this as a runtime loop the way a
/// plain `constexpr` call left un-bound (no `static constexpr`/template-
/// argument context) could. That's also what makes the `throw` below a
/// compile error rather than a runtime exception: `strategy` not actually
/// subscribed to `venue` is a composition-root wiring bug, and this way
/// it's caught at build time instead of silently returning an
/// out-of-range index.
template <std::size_t N>
consteval std::size_t venue_consumer_index(const std::array<VenueSubscription, N>& subs,
                                           std::size_t strategy, VenueId venue) {
    std::size_t index = 0;
    for (const auto& s : subs) {
        if (s.strategy == strategy && s.venue == venue) return index;
        if (s.venue == venue) ++index;
    }
    throw "venue_consumer_index: strategy is not subscribed to venue";
}

}  // namespace qp
