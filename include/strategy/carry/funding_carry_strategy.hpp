#pragma once
#include <span>
#include <variant>

#include "portfolio.hpp"
#include "strategy.hpp"
#include "types.hpp"

namespace qp::strategy::carry {

/// Parameters for FundingCarryStrategy.
struct Config {
    SymbolId symbol{};
    VenueId  spot_venue{};
    VenueId  futures_venue{};
    Qty      target_qty{1.0};
    double   entry_funding_rate{0.0001};  ///< Enter or hold once funding_rate reaches this.
    double   exit_funding_rate{0.0};      ///< Flatten once funding_rate drops to this or below.
};

/// Delta-neutral carry strategy. Holds spot long and perpetual futures
/// short on the same symbol to collect the funding payment while funding
/// is running high enough to be worth capturing.
template <PortfolioLike Book>
class FundingCarryStrategy {
   public:
    static constexpr std::size_t kMaxIntents = 2;

    FundingCarryStrategy(Config config, const Book& portfolio)
        : config_{config}, portfolio_{portfolio} {}

    /// Reacts to funding events for the configured symbol and futures
    /// venue, ignores everything else. Sizes both legs to the target
    /// quantity once funding clears the entry threshold, flattens both
    /// legs at or below the exit threshold, otherwise holds the current
    /// position.
    std::span<const Intent> on_event(const MarketEvent& event) {
        const auto* funding = std::get_if<FundingEvent>(&event);
        if (!funding || funding->symbol != config_.symbol ||
            funding->venue != config_.futures_venue) {
            return {};
        }

        // Entry checked first, since the entry threshold is never below the exit threshold.
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
    const Book&               portfolio_;  ///< Read-only. Engine owns writes.
    IntentBuffer<kMaxIntents> buffer_{};
};

static_assert(Strategy<FundingCarryStrategy<Portfolio<qp::detail::kTrivialCounts>>>);

}  // namespace qp::strategy::carry
