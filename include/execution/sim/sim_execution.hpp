#pragma once
#include <cstddef>
#include <span>
#include <utility>
#include <variant>

#include "matcher/matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"
#include "viewable_pool.hpp"

namespace qp::execution::sim {

/// Backtest ExecutionGateway: wraps a Matcher with the accumulate/drain
/// plumbing every ExecutionGateway needs, generic over fill sophistication
/// (M). 
/// Fills/rejects live in two ViewablePools, not a queue: every submit()
/// this step happens on the same thread as the drain that follows it, so
/// there's nothing for a cross-thread queue to buy. on_market_event()
/// resets both pool.
template <matcher::Matcher M, PortfolioLike Book>
class SimExecution {
    // Bounds one step's worth of outcomes: BasicRiskGate's own
    // flatten-everything bound (one order per position slot) plus headroom
    // for the same step's strategy-approved intents landing alongside a
    // trip. A generous constant, not a tight bound.
    static constexpr std::size_t kMaxOutcomes = Book::kMaxInstruments + 64;

   public:
    void on_market_event(const MarketEvent& ev) {
        reset_outcomes();
        matcher_.on_market_event(ev);
    }

    void submit(Order o, Timestamp ts) {
        auto outcome = matcher_.try_fill(o, ts);
        if (auto* fill = std::get_if<Fill>(&outcome))
            fills_.push(*fill);
        else
            rejects_.push(std::get<Reject>(outcome));
    }

    std::span<const Fill> fills() const noexcept { return fills_.view(); }

    std::span<const Reject> rejects() const noexcept { return rejects_.view(); }

    /// Clears fills()/rejects() without touching the matcher.
    /// on_market_event() calls this itself. Exposed separately so a
    /// benchmark can isolate submit()+fills() from the matcher's own cost.
    void reset_outcomes() noexcept {
        fills_.reset();
        rejects_.reset();
    }

   private:
    M                                                    matcher_;
    ViewablePool<Fill, kMaxOutcomes, /*UseHeap=*/true>   fills_;
    ViewablePool<Reject, kMaxOutcomes, /*UseHeap=*/true> rejects_;
};

}  // namespace qp::execution::sim
