#include <gtest/gtest.h>

#include "exchange.hpp"
#include "funding_carry_strategy.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::strategy::carry::Config;
using qp::strategy::carry::FundingCarryStrategy;
using qp::strategy::carry::Leg;

namespace {

constexpr SlotOffset kExchange      = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kFuturesMarket = 0;
constexpr SlotOffset kSpotMarket    = 1;
constexpr SlotOffset kSymbol        = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kFuturesMarket, "A");
    sub.add(ExchangeId::Binance, kFuturesMarket, "B");
    sub.add(ExchangeId::Binance, kSpotMarket, "A");
    sub.add(ExchangeId::Binance, kSpotMarket, "B");
    return std::move(sub).build();
}

Config make_config() {
    return Config{.futures            = Leg{kExchange, kFuturesMarket, kSymbol},
                  .spot               = Leg{kExchange, kSpotMarket, kSymbol},
                  .target_qty         = 2.0,
                  .entry_funding_rate = 0.0001,
                  .exit_funding_rate  = 0.0};
}

using Portfolio = qp::Portfolio;

}  // namespace

TEST(FundingCarryStrategy, IgnoresNonFundingEvents) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};

    auto intents = strategy.on_event(
        qp::test::make_trade(kSymbol, 0, 100.0, 1.0, qp::Side::Buy, kFuturesMarket));

    EXPECT_TRUE(intents.empty());
}

TEST(FundingCarryStrategy, IgnoresFundingEventsForAnotherSymbolOrVenue) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};

    EXPECT_TRUE(
        strategy.on_event(qp::test::make_funding(kSymbol + 1, 0, 0.001, kFuturesMarket)).empty());
    EXPECT_TRUE(strategy.on_event(qp::test::make_funding(kSymbol, 0, 0.001, kSpotMarket)).empty());
}

TEST(FundingCarryStrategy, EntersLongSpotShortFuturesWhenFundingRateClearsTheEntryThreshold) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};

    auto intents = strategy.on_event(qp::test::make_funding(kSymbol, 0, 0.0002, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_EQ(intents[0].market, kSpotMarket);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_EQ(intents[1].market, kFuturesMarket);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, EntersAtExactlyTheEntryThreshold) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};

    auto intents = strategy.on_event(qp::test::make_funding(kSymbol, 0, 0.0001, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, FlattensBothLegsWhenFundingRateDropsToTheExitThreshold) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kSpotMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Buy,
                                  .qty      = 2.0});
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kFuturesMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Sell,
                                  .qty      = 2.0});

    auto intents = strategy.on_event(qp::test::make_funding(kSymbol, 0, -0.0001, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 0.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, 0.0);
}

TEST(FundingCarryStrategy, FlattensAtExactlyTheExitThreshold) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kSpotMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Buy,
                                  .qty      = 2.0});
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kFuturesMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Sell,
                                  .qty      = 2.0});

    auto intents = strategy.on_event(qp::test::make_funding(kSymbol, 0, 0.0, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 0.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, 0.0);
}

TEST(FundingCarryStrategy, HoldsTheCurrentPositionWhenFundingRateIsBetweenTheThresholds) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kSpotMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Buy,
                                  .qty      = 2.0});
    portfolio.apply_fill(qp::Fill{.exchange = kExchange,
                                  .market   = kFuturesMarket,
                                  .symbol   = kSymbol,
                                  .side     = qp::Side::Sell,
                                  .qty      = 2.0});

    auto intents = strategy.on_event(qp::test::make_funding(kSymbol, 0, 0.00005, kFuturesMarket));

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 2.0);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -2.0);
}

TEST(FundingCarryStrategy, OnTimerEmitsNoIntents) {
    Portfolio            portfolio{make_subscription()};
    FundingCarryStrategy strategy{make_config(), portfolio};

    EXPECT_TRUE(strategy.on_timer(0).empty());
}
