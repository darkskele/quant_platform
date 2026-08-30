#pragma once
#include <cstddef>
#include <utility>

#include "spmc_ring.hpp"
#include "types.hpp"

namespace qp::data_source::sink::fanout {

/// Fan-out counterpart to FileRecorder: same write-side call shape
/// (record(MarketEvent)) as the persistence Sink, but redistributes to
/// NumConsumers in-process readers instead of writing to disk. Consumer
/// side is deliberately not built in here: a consumer's ring index is a
/// compile-time constant (qp::venue_consumer_index, core/
/// venue_subscriptions.hpp) that the wiring layer passes into whatever
/// Transport it builds around this ring, keeping source and sink from
/// depending on each other (matches D19's rule). One ring per instance —
/// a multi-venue setup (e.g. a carry strategy's spot + perp) uses one
/// FanoutSink per venue, paired with its source by run_data_source, which
/// is also what stamps MarketEvent::venue — FanoutSink itself stays
/// venue-agnostic.
///
/// Ring holds MarketEvent directly, not shared_ptr<const MarketEvent> —
/// no per-event heap allocation here anymore (was one make_shared per
/// event, regardless of kind). Every N consumers now gets its own copy on
/// pop instead of sharing one heap-allocated object: free for the flat
/// kinds (Trade/Funding/Kline — no heap members at all), a shared_ptr
/// refcount bump for BookDiff/BookSnapshot (their levels are already
/// behind their own internal shared_ptr<const BookLevels> — see
/// core/types.hpp — so even that copy doesn't deep-copy the actual price
/// levels). UseHeap=true on the ring: MarketEvent's size is dominated by
/// its widest alternative, and Capacity * sizeof(MarketEvent) living
/// inline in this object (rather than one heap block allocated once at
/// construction) isn't worth it — see SpmcRing's own UseHeap comment.
template <std::size_t Capacity, std::size_t NumConsumers>
class FanoutSink {
   public:
    using Ring = SpmcRing<MarketEvent, Capacity, NumConsumers, /*UseHeap=*/true>;

    /// Unlike FileRecorder::record (non-blocking, drops on a full queue),
    /// this can genuinely block the caller: the ring is gated and never
    /// drops (spmc_ring.hpp) — a stalled consumer backpressures here.
    void record(MarketEvent event) { ring_.push(std::move(event)); }

    Ring& ring() noexcept { return ring_; }

   private:
    Ring ring_;
};

}  // namespace qp::data_source::sink::fanout
