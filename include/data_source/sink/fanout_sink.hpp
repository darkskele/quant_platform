#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>

#include "spmc_ring.hpp"
#include "types.hpp"

namespace qp::sink {

/// Fan-out counterpart to FileRecorder: same write-side call shape
/// (record(MarketEvent)) as the persistence Sink, but redistributes to
/// NumConsumers in-process readers instead of writing to disk. Consumer
/// side is deliberately not built in here: attach() only hands out a
/// cursor id; InProcessTransport (libs/data_source/transport) is built
/// around it at the wiring layer, keeping source and sink from depending
/// on each other (matches D19's rule). One ring per instance — a
/// multi-venue setup (e.g. a carry strategy's spot + perp) uses one
/// FanoutSink per venue, paired with its source by run_data_source
/// (libs/data_source/include/run_data_source.hpp, D43), which is also
/// what stamps MarketEvent::venue — FanoutSink itself stays venue-agnostic.
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

    /// Hands out the next cursor id (0..NumConsumers-1) for a new reader
    /// to attach a Transport to. No detach — the consumer set is fixed for
    /// the ring's lifetime, same as SpmcRing itself.
    std::size_t attach() {
        auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
        if (id >= NumConsumers) {
            throw std::out_of_range("FanoutSink: attach() exceeds NumConsumers");
        }
        return id;
    }

    Ring& ring() noexcept { return ring_; }

   private:
    Ring                     ring_;
    std::atomic<std::size_t> next_id_{0};
};

}  // namespace qp::sink
