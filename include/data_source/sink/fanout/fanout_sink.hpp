#pragma once
#include <cstddef>
#include <utility>

#include "spmc_queue.hpp"
#include "types.hpp"

namespace qp::data_source::sink::fanout {

/// Redistributes record()ed events to NumConsumers in-process readers.
/// A multi-venue setup uses one FanoutSink per venue, paired with its
/// source by run_data_source, which is also what stamps MarketEvent::venue.
template <std::size_t Capacity, std::size_t NumConsumers>
class FanoutSink {
   public:
    using Queue = SpmcQueue<MarketEvent, Capacity, NumConsumers, /*UseHeap=*/true>;

    /// Never blocks: returns false if the queue isn't ready to reuse its
    /// next slot yet. Retry, drop, or wait is the caller's decision.
    /// Consumes `event` only on success.
    bool record(MarketEvent&& event) { return queue_.push(std::move(event)); }

    Queue& queue() noexcept { return queue_; }

   private:
    Queue queue_;
};

}  // namespace qp::data_source::sink::fanout
