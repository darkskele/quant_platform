#include <gtest/gtest.h>

#include "funding_carry_strategy.hpp"
#include "portfolio.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::strategy::carry::Config;
using qp::strategy::carry::FundingCarryStrategy;

namespace {

constexpr qp::SymbolId kSymbol       = 1;
constexpr qp::VenueId  kSpotVenue    = 0;
constexpr qp::VenueId  kFuturesVenue = 1;

Config make_config() {
    return Config{.symbol             = kSymbol,
                  .spot_venue         = kSpotVenue,
                  .futures_venue      = kFuturesVenue,
                  .target_qty         = 2.0,
                  .entry_funding_rate = 0.0001,
                  .exit_funding_rate  = 0.0};
}

}  // namespace

TEST(FundingCarryStrategy, IgnoresNonFundingEvents) {
    FundingCarryStrategy strategy{make_config()};
    qp::Portfolio        portfolio;

    auto intents = strategy.on_event(qp::test::make_trade(kSymbol, 0, 100.0), portfolio.view());

    EXPECT_TRUE(intents.empty());
}

TEST(FundingCarryStrategy, IgnoresFundingEventsForAnotherSymbolOrVenue) {
    FundingCarryStrategy strategy{make_config()};
    qp::Portfolio        portfolio;

    EXPECT_TRUE(strategy
                    .on_event(qp::test::make_funding(kSymbol + 1, 0, 0.001, 0.0, kFuturesVenue),
                              portfolio.view())
                    .empty());
    EXPECT_TRUE(
        strategy
            .on_event(qp::test::make_funding(kSymbol, 0, 0.001, 0.0, kSpotVenue), portfolio.view())
            .empty());
}

TEST(FundingCarryStrategy, EntersLongSpotShortFuturesWhenFundingRateClearsTheEntryThreshold) {
    FundingCarryStrategy strategy{make_config()};
    qp::Portfolio        portfolio;

    auto intents = strategy.on_event(
        qp::test::make_funding(kSymbol, 0, /*rate=*/0.0002, 0.0, kFuturesVenue), portfolio.view());

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_EQ(intents[0].venue, kSpotVenue);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_EQ(intents[1].venue, kFuturesVenue);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, FlattensBothLegsWhenFundingRateDropsToTheExitThreshold) {
    FundingCarryStrategy strategy{make_config()};
    qp::Portfolio        portfolio;
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .venue = kSpotVenue, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .venue = kFuturesVenue, .qty = 2.0});

    auto intents = strategy.on_event(
        qp::test::make_funding(kSymbol, 0, /*rate=*/-0.0001, 0.0, kFuturesVenue), portfolio.view());

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 0.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, 0.0);
}

TEST(FundingCarryStrategy, HoldsTheCurrentPositionWhenFundingRateIsBetweenTheThresholds) {
    FundingCarryStrategy strategy{make_config()};
    qp::Portfolio        portfolio;
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .venue = kSpotVenue, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .venue = kFuturesVenue, .qty = 2.0});

    auto intents = strategy.on_event(
        qp::test::make_funding(kSymbol, 0, /*rate=*/0.00005, 0.0, kFuturesVenue), portfolio.view());

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, OnTimerEmitsNoIntents) {
    FundingCarryStrategy strategy{make_config()};
    qp::Portfolio        portfolio;

    EXPECT_TRUE(strategy.on_timer(0, portfolio.view()).empty());
}
