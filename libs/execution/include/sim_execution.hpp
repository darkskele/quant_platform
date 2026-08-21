#pragma once
#include <optional>
#include <queue>
#include <utility>
#include <variant>

#include "matcher.hpp"
#include "types.hpp"

namespace qp::execution {

/// Backtest ExecutionGateway: wraps a Matcher with the queue/poll plumbing
/// every ExecutionGateway needs, generic over fill sophistication (M).
/// LiveExecution doesn't share this template — its submit() is
/// fundamentally async (fires an order, a background thread fills the
/// outcome queue whenever the exchange responds), not "compute inline," so
/// forcing it through the same shell would just be a synchronous
/// abstraction wearing an async mask. Two separate ExecutionGateway
/// implementations, same concept, same as LiveWebSocketSource/
/// FileReplaySource under Source.
template <Matcher M>
class SimExecution {
   public:
    void on_market_event(MarketEvent ev) { matcher_.on_market_event(std::move(ev)); }

    void submit(Order o, Timestamp ts) { outcomes_.push(matcher_.try_fill(o, ts)); }

    std::optional<std::variant<Fill, Reject>> next_outcome() {
        if (outcomes_.empty()) return std::nullopt;
        auto out = std::move(outcomes_.front());
        outcomes_.pop();
        return out;
    }

   private:
    M                                      matcher_;
    std::queue<std::variant<Fill, Reject>> outcomes_;
};

}  // namespace qp::execution
