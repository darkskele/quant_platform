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
/// (M). LiveExecution doesn't share this template — its submit() is
/// fundamentally async (fires an order, a background thread fills the
/// outcome queue whenever the exchange responds), not "compute inline," so
/// forcing it through the same shell would just be a synchronous
/// abstraction wearing an async mask. Two separate ExecutionGateway
/// implementations, same concept, same as LiveWebSocketSource/
/// FileReplaySource under Source.
///
/// Fills/rejects live in two ViewablePools, not a queue: every submit()
/// this step happens on the same thread as the drain that follows it
/// (Engine::step() calls on_market_event -> submits -> fills()/rejects(),
/// never spanning threads), so there's nothing here for a heap-backed
/// queue or cross-thread SpscQueue to buy — a stack/reset scratch buffer
/// is strictly cheaper. on_market_event() resets both pools: it's the one
/// call Engine makes exactly once per step, before that step's submits, so
/// it's the natural "last step's outcomes are stale" boundary.
template <matcher::Matcher M, PortfolioLike Book>
class SimExecution {
    // Bounds one step's worth of outcomes: BasicRiskGate's own on_tick
    // flatten-everything bound (Book::kMaxInstruments, one order per
    // position slot) plus headroom for the same step's strategy-approved
    // intents landing alongside a trip. Composition-wide (multi-strategy)
    // sizing isn't derived here — SimExecution has no visibility into which
    // Strategies it's paired with — so this is a deliberately generous
    // constant, not a tight bound; revisit if a composition's total
    // per-step submits can plausibly exceed it.
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

    /// Clears fills()/rejects() without touching the matcher — on_market_event()
    /// calls this itself; exposed separately so a benchmark can isolate
    /// submit()+fills() from the matcher's own on_market_event() cost
    /// (already measured on its own, see bench_last_trade_matcher.cpp).
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
