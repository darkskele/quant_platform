#pragma once
#include <cstddef>
#include <memory>
#include <optional>

#include "types.hpp"

namespace qp::source {

/// Source-shaped reader over one fixed cursor of a fan-out ring. Consumer
/// is compile-time, not a stored runtime index — fixed at the wiring call
/// site instead of handed out via FanoutSink::attach().
template <typename Ring, std::size_t Consumer>
class InProcessTransport {
   public:
    explicit InProcessTransport(Ring& ring) : ring_(&ring) {}

    std::optional<std::shared_ptr<const MarketEvent>> next() {
        return ring_->template try_pop<Consumer>();
    }

   private:
    Ring* ring_;
};

}  // namespace qp::source
