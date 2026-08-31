#pragma once
#include <cstddef>
#include <utility>

#include "spmc_ring.hpp"
#include "types.hpp"

namespace qp::data_source::sink::fanout {

/// Redistributes record()ed events to NumConsumers in-process readers.
/// A multi-venue setup uses one FanoutSink per venue, paired with its 
/// source by run_data_source, which is also what stamps MarketEvent::venue.
/// Ring holds MarketEvent directly. Each consumer gets its own copy on pop:
/// free for the flat kinds (no heap members at all), a shared_ptr
/// refcount bump for BookDiff/BookSnapshot (their levels already live
/// behind their own shared_ptr<const BookLevels>.
template <std::size_t Capacity, std::size_t NumConsumers>
class FanoutSink {
   public:
    using Ring = SpmcRing<MarketEvent, Capacity, NumConsumers, /*UseHeap=*/true>;

    /// Can genuinely block the caller: the ring is gated and never drops,
    /// so a stalled consumer backpressures here.
    void record(MarketEvent event) { ring_.push(std::move(event)); }

    Ring& ring() noexcept { return ring_; }

   private:
    Ring ring_;
};

}  // namespace qp::data_source::sink::fanout
