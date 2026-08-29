#pragma once
#include <cstddef>
#include <memory>

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
template <std::size_t Capacity, std::size_t NumConsumers>
class FanoutSink {
   public:
    using Ring = SpmcRing<std::shared_ptr<const MarketEvent>, Capacity, NumConsumers>;

    /// Unlike FileRecorder::record (non-blocking, drops on a full queue),
    /// this can genuinely block the caller: the ring is gated and never
    /// drops (spmc_ring.hpp) — a stalled consumer backpressures here.
    void record(MarketEvent event) {
        ring_.push(std::make_shared<const MarketEvent>(std::move(event)));
    }

    Ring& ring() noexcept { return ring_; }

   private:
    Ring ring_;
};

}  // namespace qp::data_source::sink::fanout
