#pragma once
#include <cstddef>
#include <optional>

#include "types.hpp"

namespace qp::transport {

/// Transport-shaped reader over one fixed cursor of a fan-out ring
/// (SpmcRing, libs/core). Consumer is compile-time, not a stored runtime
/// index — fixed at the wiring call site instead of handed out via
/// FanoutSink::attach().
///
/// Rings store shared_ptr<const MarketEvent> (one allocation shared by
/// refcount across every consumer, not copied per consumer — the whole
/// point of the ring). next() dereferences and copies out a plain
/// MarketEvent, one copy per event *this* consumer pulls, because that's
/// what Transport requires — Engine wants a value, not a shared_ptr. Real
/// cost, paid once per event per reader, not multiplied by ring depth or
/// consumer count.
template <typename Ring, std::size_t Consumer>
class InProcessTransport {
   public:
    explicit InProcessTransport(Ring& ring) : ring_(&ring) {}

    std::optional<MarketEvent> next() {
        auto ptr = ring_->template try_pop<Consumer>();
        if (!ptr) return std::nullopt;
        return **ptr;
    }

   private:
    Ring* ring_;
};

}  // namespace qp::transport
