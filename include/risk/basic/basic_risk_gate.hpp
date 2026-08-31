#pragma once
#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "types.hpp"
#include "viewable_pool.hpp"

namespace qp::risk::basic {

/// A (symbol, venue) pair considered for flattening when the kill switch trips.
struct TrackedInstrument {
    SymbolId symbol{};
    VenueId  venue{};
};

/// Parameters for BasicRiskGate.
struct BasicRiskGateConfig {
    Qty                            max_position_qty{10.0};  ///< Per (symbol, venue) exposure cap.
    Notional                       max_drawdown{50.0};      ///< Trips the kill switch past this.
    std::vector<TrackedInstrument> tracked{};               ///< Flattened once tripped.
};

/// Per-(symbol, venue) exposure cap plus an equity-drawdown kill switch.
template <PortfolioLike Book>
class BasicRiskGate {
   public:
    BasicRiskGate(BasicRiskGateConfig config, const Book& portfolio)
        : config_{std::move(config)}, portfolio_{portfolio} {}

    /// Sizes an Order against the current position, clamped to
    /// max_position_qty. Rejects outright once tripped.
    RiskDecision check(Intent intent) {
        if (tripped_) return {.outcome = RiskOutcome::Rejected, .order = std::nullopt};

        Qty current = portfolio_.position(intent.symbol, intent.venue);
        Qty target =
            std::clamp(intent.target_position, -config_.max_position_qty, config_.max_position_qty);
        Qty delta = target - current;

        auto outcome =
            target == intent.target_position ? RiskOutcome::Approved : RiskOutcome::Resized;
        return {outcome, Order{.id     = next_id(),
                               .symbol = intent.symbol,
                               .side   = delta >= 0 ? Side::Buy : Side::Sell,
                               .venue  = intent.venue,
                               .qty    = std::abs(delta)}};
    }

    /// Trips and flattens every tracked position once equity has declined
    /// past max_drawdown off its peak. No-op once already tripped.
    std::span<const Order> on_tick() {
        Notional equity = portfolio_.equity();
        peak_equity_    = std::max(peak_equity_, equity);
        if (tripped_ || peak_equity_ - equity < config_.max_drawdown) return {};

        tripped_ = true;
        orders_.reset();
        for (const auto& [symbol, venue] : config_.tracked) {
            Qty pos = portfolio_.position(symbol, venue);
            if (pos == 0.0) continue;
            orders_.push(Order{.id     = next_id(),
                               .symbol = symbol,
                               .side   = pos > 0 ? Side::Sell : Side::Buy,
                               .venue  = venue,
                               .qty    = std::abs(pos)});
        }
        return orders_.view();
    }

   private:
    static constexpr std::size_t kMaxOrders =
        Book::kMaxInstruments;  ///< Worst case: all flatten at once.

    OrderId next_id() noexcept { return next_id_++; }

    BasicRiskGateConfig config_;
    const Book&         portfolio_;  ///< Read-only. Engine owns writes.
    OrderId             next_id_{1};
    bool                tripped_{false};
    Notional            peak_equity_{0.0};
    ViewablePool<Order, kMaxOrders, /*UseHeap=*/true> orders_;
};

static_assert(RiskGate<BasicRiskGate<Portfolio<qp::detail::kTrivialCounts>>>);

}  // namespace qp::risk::basic
