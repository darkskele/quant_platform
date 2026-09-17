#pragma once
#include <span>
#include <variant>

#include "portfolio.hpp"
#include "strategy.hpp"
#include "types.hpp"

namespace qp::strategy::carry {

struct Leg {
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
};

struct Config {
    Leg    futures{};
    Leg    spot{};
    Qty    target_qty{1.0};
    double entry_funding_rate{0.0001};
    double exit_funding_rate{0.0};
};

class FundingCarryStrategy {
   public:
    static constexpr std::size_t kMaxIntents = 2;

    FundingCarryStrategy(Config config, const Portfolio& portfolio)
        : config_{config}, portfolio_{&portfolio} {}

    std::span<const Intent> on_event(const MarketEvent& event) {
        const auto* funding = std::get_if<FundingEvent>(&event.payload);
        if (!funding || event.base.exchange != config_.futures.exchange ||
            event.base.market != config_.futures.market ||
            event.base.symbol != config_.futures.symbol) {
            return {};
        }

        Qty target = funding->funding_rate >= config_.entry_funding_rate ? config_.target_qty
                     : funding->funding_rate <= config_.exit_funding_rate
                         ? 0.0
                         : portfolio_->position(config_.spot.exchange, config_.spot.market,
                                                config_.spot.symbol);

        buffer_.reset();
        buffer_.push(Intent{
            .exchange        = config_.spot.exchange,
            .market          = config_.spot.market,
            .symbol          = config_.spot.symbol,
            .target_position = target,
        });
        buffer_.push(Intent{
            .exchange        = config_.futures.exchange,
            .market          = config_.futures.market,
            .symbol          = config_.futures.symbol,
            .target_position = -target,
        });
        return buffer_.view();
    }

    std::span<const Intent> on_timer(Timestamp) { return {}; }

   private:
    Config                    config_;
    const Portfolio*          portfolio_;
    IntentBuffer<kMaxIntents> buffer_{};
};

static_assert(Strategy<FundingCarryStrategy>);

}  // namespace qp::strategy::carry
