#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "types.hpp"

namespace qp::risk {

/// Runtime, backtest-tuned parameters — see FundingCarryStrategy::Config
/// for the same compile-time-vs-runtime reasoning (CLAUDE.md).
struct BasicRiskGateConfig {
    Qty      max_position_qty{10.0};  ///< Per (symbol, venue) absolute exposure cap.
    Notional max_drawdown{50.0};  ///< Absolute equity decline from peak that trips the kill switch.
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
class BasicRiskGate {
   public:
    explicit BasicRiskGate(BasicRiskGateConfig config) : config_{config} {}

    RiskDecision check(Intent intent, StateView state) {
        if (tripped_) return {.outcome = RiskOutcome::Rejected, .order = std::nullopt};

        known_[index(intent.symbol, intent.venue)] = true;

        Qty current = state.position(intent.symbol, intent.venue);
        Qty target =
            std::clamp(intent.target_position, -config_.max_position_qty, config_.max_position_qty);
        Qty delta = target - current;

        Order order{.id     = next_id(),
                    .symbol = intent.symbol,
                    .side   = delta >= 0 ? Side::Buy : Side::Sell,
                    .venue  = intent.venue,
                    .qty    = std::abs(delta)};

        auto outcome =
            target == intent.target_position ? RiskOutcome::Approved : RiskOutcome::Resized;
        return {outcome, order};
    }

    std::vector<Order> on_tick(StateView state) {
        peak_equity_ = std::max(peak_equity_, state.equity());
        if (tripped_ || peak_equity_ - state.equity() < config_.max_drawdown) return {};

        tripped_ = true;
        std::vector<Order> orders;
        for (SymbolId s = 0; s < Portfolio::kMaxSymbols; ++s) {
            for (VenueId v = 0; v < Portfolio::kMaxVenues; ++v) {
                if (!known_[index(s, v)]) continue;
                Qty pos = state.position(s, v);
                if (pos == 0.0) continue;
                orders.push_back(Order{.id     = next_id(),
                                       .symbol = s,
                                       .side   = pos > 0 ? Side::Sell : Side::Buy,
                                       .venue  = v,
                                       .qty    = std::abs(pos)});
            }
        }
        return orders;
    }

   private:
    static constexpr std::size_t index(SymbolId symbol, VenueId venue) noexcept {
        return static_cast<std::size_t>(symbol) * Portfolio::kMaxVenues + venue;
    }

    OrderId next_id() noexcept { return next_id_++; }

    BasicRiskGateConfig                                              config_;
    OrderId                                                          next_id_{1};
    bool                                                             tripped_{false};
    Notional                                                         peak_equity_{0.0};
    std::array<bool, Portfolio::kMaxSymbols * Portfolio::kMaxVenues> known_{};
};

static_assert(RiskGate<BasicRiskGate>);

}  // namespace qp::risk
