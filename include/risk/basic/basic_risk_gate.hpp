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

/// A (symbol, venue) pair the kill switch should consider flattening —
/// see BasicRiskGateConfig::tracked.
struct TrackedInstrument {
    SymbolId symbol{};
    VenueId  venue{};
};

/// Runtime, backtest-tuned parameters — see FundingCarryStrategy::Config
/// for the same compile-time-vs-runtime reasoning (CLAUDE.md).
struct BasicRiskGateConfig {
    Qty      max_position_qty{10.0};  ///< Per (symbol, venue) absolute exposure cap.
    Notional max_drawdown{50.0};  ///< Absolute equity decline from peak that trips the kill switch.
    std::vector<TrackedInstrument> tracked{};  ///< Flatten candidates on trip — the caller's own
                                               ///< strategy config already knows which (symbol,
                                               ///< venue) pairs are traded; on_tick() only walks
                                               ///< this list, not every possible pair.
};

/// First concrete RiskGate (D46): a per-(symbol, venue) exposure cap
/// (check(), Resized when clamped) plus an equity-drawdown kill switch
/// (on_tick(), D46's Portfolio::equity()). Deliberately not a fraction of
/// peak equity — Portfolio has no allocated-starting-capital concept, so
/// peak equity starts at/near 0 and a %-based threshold would divide by a
/// near-zero or negative denominator early in a run; an absolute Notional
/// decline is the honest metric given what's actually modeled.
///
/// Once tripped, stays tripped: every subsequent check() rejects, on_tick()
/// stops re-flattening. A real kill switch doesn't quietly resume on its
/// own — that's an operator decision, not this class's to make.
///
/// Templated on Book (PortfolioLike), held by reference: the composition
/// root wires this to the same Book instance the paired Engine holds, so a
/// drawdown kill switch shared across several single-strategy Engines
/// (one account, several strategies) sees the account's true combined
/// equity, not a snapshot scoped to whichever Engine last called check().
template <PortfolioLike Book>
class BasicRiskGate {
   public:
    BasicRiskGate(BasicRiskGateConfig config, Book& portfolio)
        : config_{std::move(config)}, portfolio_{portfolio} {}

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
    static constexpr std::size_t kMaxOrders = Book::kMaxSymbols * Book::kMaxVenues;

    OrderId next_id() noexcept { return next_id_++; }

    BasicRiskGateConfig                               config_;
    Book&                                             portfolio_;
    OrderId                                           next_id_{1};
    bool                                              tripped_{false};
    Notional                                          peak_equity_{0.0};
    ViewablePool<Order, kMaxOrders, /*UseHeap=*/true> orders_;
};

static_assert(RiskGate<BasicRiskGate<Portfolio>>);

}  // namespace qp::risk::basic
