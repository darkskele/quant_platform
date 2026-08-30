#pragma once
#include <span>
#include <variant>

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
/// the sense that matters: every decision reads the current position off
/// `portfolio_` rather than tracking its own, so the same MarketEvent
/// always produces the same Intent regardless of which strategy instance
/// runs it. `buffer_` is pure scratch output space, not decision state.
/// Templated on Book (PortfolioLike), held by `const` reference — same
/// object Engine holds, wired by the composition root at construction;
/// const because this strategy only ever reads it.
template <PortfolioLike Book>
class FundingCarryStrategy {
   public:
    static constexpr std::size_t kMaxIntents = 2;

    FundingCarryStrategy(Config config, const Book& portfolio)
        : config_{config}, portfolio_{portfolio} {}

    std::span<const Intent> on_event(const MarketEvent& event) {
        const auto* funding = std::get_if<FundingEvent>(&event);
        if (!funding || funding->symbol != config_.symbol ||
            funding->venue != config_.futures_venue) {
            return {};
        }

        Qty target = funding->funding_rate >= config_.entry_funding_rate ? config_.target_qty
                     : funding->funding_rate <= config_.exit_funding_rate
                         ? 0.0
                         : portfolio_.position(config_.symbol, config_.spot_venue);

        buffer_.reset();
        buffer_.push(Intent{
            .symbol = config_.symbol, .venue = config_.spot_venue, .target_position = target});
        buffer_.push(Intent{
            .symbol = config_.symbol, .venue = config_.futures_venue, .target_position = -target});
        return buffer_.view();
    }

    std::span<const Intent> on_timer(Timestamp) { return {}; }

   private:
    Config                    config_;
    const Book&               portfolio_;
    IntentBuffer<kMaxIntents> buffer_{};
};

static_assert(Strategy<FundingCarryStrategy<Portfolio>>);

}  // namespace qp::strategy::carry
