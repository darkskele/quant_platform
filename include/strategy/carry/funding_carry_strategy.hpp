#pragma once
#include <span>

#include "portfolio.hpp"
#include "strategy.hpp"
#include "types.hpp"

namespace qp::strategy::carry {

/// Runtime, backtest-swept parameters — defaults are placeholders, refined
/// by backtesting against historical funding/kline data, not hand-picked.
struct Config {
    SymbolId symbol{};
    VenueId  spot_venue{};
    VenueId  futures_venue{};
    Qty      target_qty{1.0};
    Notional entry_funding_rate{0.0001};  ///< Enter (or hold) once funding_rate reaches this.
    Notional exit_funding_rate{0.0};      ///< Flatten once funding_rate drops to this or below.
};

/// Long spot + short perp, delta-neutral, collecting funding. Stateless in
/// the sense that matters: every decision reads StateView's current
/// position rather than tracking its own, so the same MarketEvent always
/// produces the same Intent regardless of which strategy instance/thread
/// runs it. `buffer_` is pure scratch output space, not decision state.
class FundingCarryStrategy {
   public:
    static constexpr std::size_t kMaxIntents = 2;

    explicit FundingCarryStrategy(Config config) : config_{config} {}

    std::span<const Intent> on_event(const MarketEvent& event, StateView state) {
        if (event.kind != EventKind::Funding || event.symbol != config_.symbol ||
            event.venue != config_.futures_venue) {
            return {};
        }

        Qty target = event.funding_rate >= config_.entry_funding_rate ? config_.target_qty
                     : event.funding_rate <= config_.exit_funding_rate
                         ? 0.0
                         : state.position(config_.symbol, config_.spot_venue);

        buffer_.reset();
        buffer_.push(Intent{
            .symbol = config_.symbol, .venue = config_.spot_venue, .target_position = target});
        buffer_.push(Intent{
            .symbol = config_.symbol, .venue = config_.futures_venue, .target_position = -target});
        return buffer_.view();
    }

    std::span<const Intent> on_timer(Timestamp, StateView) { return {}; }

   private:
    Config                    config_;
    IntentBuffer<kMaxIntents> buffer_{};
};

static_assert(Strategy<FundingCarryStrategy>);

}  // namespace qp::strategy::carry
