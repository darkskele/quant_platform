#pragma once
#include <cstddef>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "matcher/matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim {

/// Backtest ExecutionGateway.
template <matcher::Matcher M>
class SimExecution {
   public:
    SimExecution(const Portfolio& book, M matcher) noexcept : matcher_(std::move(matcher)) {
        // BasicRiskGate's own flatten-everything bound.
        const std::size_t cap = book.max_instruments() + 64;
        fills_.reserve(cap);
        rejects_.reserve(cap);
    }

    void on_market_event(const MarketEvent& ev) {
        reset_outcomes();
        matcher_.on_market_event(ev);
    }

    void submit(Order o, Timestamp ts) {
        auto outcome = matcher_.try_fill(o, ts);
        if (auto* fill = std::get_if<Fill>(&outcome))
            fills_.push_back(*fill);
        else
            rejects_.push_back(std::get<Reject>(outcome));
    }

    std::span<const Fill> fills() const noexcept { return {fills_.data(), fills_.size()}; }

    std::span<const Reject> rejects() const noexcept { return {rejects_.data(), rejects_.size()}; }

    /// Clears fills()/rejects() without touching the matcher.
    void reset_outcomes() noexcept {
        fills_.clear();
        rejects_.clear();
    }

   private:
    M                   matcher_;
    std::vector<Fill>   fills_;
    std::vector<Reject> rejects_;
};

}  // namespace qp::execution::sim
