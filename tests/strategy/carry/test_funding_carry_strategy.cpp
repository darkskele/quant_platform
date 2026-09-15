#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include "funding_carry_strategy.hpp"
#include "portfolio.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::strategy::carry::Config;
using qp::strategy::carry::FundingCarryStrategy;

namespace {

constexpr std::array<std::size_t, 2> kCounts{2, 2};
using Portfolio = qp::Portfolio;

constexpr qp::SymbolId kSymbol        = 1;
constexpr qp::Market   kSpotMarket    = qp::Market::BinanceUsdm;
constexpr qp::Market   kFuturesMarket = qp::Market::BinanceCoinm;

Config make_config() {
    return Config{.symbol             = kSymbol,
                  .spot_market        = kSpotMarket,
                  .futures_market     = kFuturesMarket,
                  .target_qty         = 2.0,
                  .entry_funding_rate = 0.0001,
                  .exit_funding_rate  = 0.0};
}

}  // namespace

TEST(FundingCarryStrategy, IgnoresNonFundingEvents) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};

    auto intents = strategy.on_event(qp::test::make_trade(kSymbol, 0, 100.0));

    EXPECT_TRUE(intents.empty());
}

TEST(FundingCarryStrategy, IgnoresFundingEventsForAnotherSymbolOrVenue) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};

    EXPECT_TRUE(
        strategy.on_event(qp::test::make_funding(kSymbol + 1, 0, 0.001, kFuturesMarket)).empty());
    EXPECT_TRUE(strategy.on_event(qp::test::make_funding(kSymbol, 0, 0.001, kSpotMarket)).empty());
}

TEST(FundingCarryStrategy, EntersLongSpotShortFuturesWhenFundingRateClearsTheEntryThreshold) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};

    auto intents =
        strategy.on_event(qp::test::make_funding(kSymbol, 0, /*rate=*/0.0002, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_EQ(intents[0].market, kSpotMarket);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_EQ(intents[1].market, kFuturesMarket);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, EntersAtExactlyTheEntryThreshold) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};

    auto intents =
        strategy.on_event(qp::test::make_funding(kSymbol, 0, /*rate=*/0.0001, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, FlattensBothLegsWhenFundingRateDropsToTheExitThreshold) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .market = kSpotMarket, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .market = kFuturesMarket, .qty = 2.0});

    auto intents =
        strategy.on_event(qp::test::make_funding(kSymbol, 0, /*rate=*/-0.0001, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 0.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, 0.0);
}

TEST(FundingCarryStrategy, FlattensAtExactlyTheExitThreshold) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .market = kSpotMarket, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .market = kFuturesMarket, .qty = 2.0});

    auto intents =
        strategy.on_event(qp::test::make_funding(kSymbol, 0, /*rate=*/0.0, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 0.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, 0.0);
}

TEST(FundingCarryStrategy, HoldsTheCurrentPositionWhenFundingRateIsBetweenTheThresholds) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Buy, .market = kSpotMarket, .qty = 2.0});
    portfolio.apply_fill(
        qp::Fill{.symbol = kSymbol, .side = qp::Side::Sell, .market = kFuturesMarket, .qty = 2.0});

    auto intents =
        strategy.on_event(qp::test::make_funding(kSymbol, 0, /*rate=*/0.00005, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, OnTimerEmitsNoIntents) {
    Portfolio            portfolio{kCounts};
    FundingCarryStrategy strategy{make_config(), portfolio};

    EXPECT_TRUE(strategy.on_timer(0).empty());
}
