#pragma once
#include <vector>

#include "portfolio.hpp"
#include "strategy.hpp"
#include "types.hpp"

namespace qp::strategy::carry {

/// Runtime, backtest-swept parameters — an open-ended, per-run-supplied
/// value belongs as a constructor argument, not a template one (unlike
/// e.g. AlignmentRule, a closed compile-time policy choice). Defaults are
/// placeholders, refined by backtesting against historical funding/kline
/// data, not hand-picked.
struct Config {
    SymbolId symbol{};
    VenueId  spot_venue{};
    VenueId  futures_venue{};
    Qty      target_qty{1.0};
    Notional entry_funding_rate{0.0001};  ///< Enter (or hold) once funding_rate reaches this.
    Notional exit_funding_rate{0.0};      ///< Flatten once funding_rate drops to this or below.
};

/// Long spot + short perp, delta-neutral, collecting funding
/// (docs/strategy.md family 1, D6). Stateless: every decision reads
/// StateView's current position rather than tracking its own, so the same
/// MarketEvent always produces the same Intent regardless of which
/// strategy instance/thread runs it.
class FundingCarryStrategy {
   public:
    explicit FundingCarryStrategy(Config config) : config_{config} {}

    std::vector<Intent> on_event(const MarketEvent& event, StateView state) {
        if (event.kind != EventKind::Funding || event.symbol != config_.symbol ||
            event.venue != config_.futures_venue) {
            return {};
        }

        Qty target;
        if (event.funding_rate >= config_.entry_funding_rate) {
            target = config_.target_qty;
        } else if (event.funding_rate <= config_.exit_funding_rate) {
            target = 0.0;
        } else {
            target = state.position(config_.symbol, config_.spot_venue);
        }

        return {
            Intent{
                .symbol = config_.symbol, .venue = config_.spot_venue, .target_position = target},
            Intent{.symbol          = config_.symbol,
                   .venue           = config_.futures_venue,
                   .target_position = -target},
        };
    }

    std::vector<Intent> on_timer(Timestamp, StateView) { return {}; }

   private:
    Config config_;
};

static_assert(Strategy<FundingCarryStrategy>);

}  // namespace qp::strategy::carry
