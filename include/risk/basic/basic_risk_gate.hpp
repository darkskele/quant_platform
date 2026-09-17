#pragma once
#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "types.hpp"

namespace qp::risk::basic {

struct TrackedInstrument {
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
};

struct BasicRiskGateConfig {
    Qty                            max_position_qty{10.0};
    Notional                       max_drawdown{50.0};
    std::vector<TrackedInstrument> tracked{};
};

class BasicRiskGate {
   public:
    BasicRiskGate(BasicRiskGateConfig config, const Portfolio& portfolio)
        : config_{std::move(config)}, portfolio_{&portfolio} {
        orders_.reserve(portfolio.max_instruments());
    }

    RiskDecision check(Intent intent) {
        if (tripped_) return {.outcome = RiskOutcome::Rejected, .order = std::nullopt};

        Qty current = portfolio_->position(intent.exchange, intent.market, intent.symbol);
        Qty target =
            std::clamp(intent.target_position, -config_.max_position_qty, config_.max_position_qty);
        Qty delta = target - current;

        auto outcome =
            target == intent.target_position ? RiskOutcome::Approved : RiskOutcome::Resized;
        return {outcome, Order{.id       = next_id(),
                               .exchange = intent.exchange,
                               .market   = intent.market,
                               .symbol   = intent.symbol,
                               .side     = delta >= 0 ? Side::Buy : Side::Sell,
                               .qty      = std::abs(delta)}};
    }

    std::span<const Order> on_tick() {
        Notional equity = portfolio_->equity();
        peak_equity_    = std::max(peak_equity_, equity);
        if (tripped_ || peak_equity_ - equity < config_.max_drawdown) return {};

        tripped_ = true;
        orders_.clear();
        for (const auto& t : config_.tracked) {
            Qty pos = portfolio_->position(t.exchange, t.market, t.symbol);
            if (pos == 0.0) continue;
            orders_.push_back(Order{.id       = next_id(),
                                    .exchange = t.exchange,
                                    .market   = t.market,
                                    .symbol   = t.symbol,
                                    .side     = pos > 0 ? Side::Sell : Side::Buy,
                                    .qty      = std::abs(pos)});
        }
        return {orders_.data(), orders_.size()};
    }

   private:
    OrderId next_id() noexcept { return next_id_++; }

    BasicRiskGateConfig config_;
    const Portfolio*    portfolio_;
    OrderId             next_id_{1};
    bool                tripped_{false};
    Notional            peak_equity_{0.0};
    std::vector<Order>  orders_;
};

static_assert(RiskGate<BasicRiskGate>);

}  // namespace qp::risk::basic
