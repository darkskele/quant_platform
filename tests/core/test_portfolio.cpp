#include <gtest/gtest.h>

#include "exchange.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::test::make_book_diff;
using qp::test::make_fill;
using qp::test::make_funding;
using qp::test::make_mark_price_kline;
using qp::test::make_trade;

namespace {
constexpr SlotOffset kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kUsdm     = 0;
constexpr SlotOffset kCoinm    = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kUsdm, "A");
    sub.add(ExchangeId::Binance, kUsdm, "B");
    sub.add(ExchangeId::Binance, kUsdm, "C");
    sub.add(ExchangeId::Binance, kCoinm, "A");
    sub.add(ExchangeId::Binance, kCoinm, "B");
    sub.add(ExchangeId::Binance, kCoinm, "C");
    return std::move(sub).build();
}

using Portfolio = qp::Portfolio;
}  // namespace

TEST(Portfolio, FlatUntilAnyFillArrives) {
    Portfolio portfolio{make_subscription()};
    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 1), 0.0);
}

TEST(Portfolio, BuyIncreasesSellDecreasesPosition) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 1), 2.0);

    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 0.5));
    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 1), 1.5);
}

TEST(Portfolio, RepeatedFillsAccumulate) {
    Portfolio portfolio{make_subscription()};
    for (int i = 0; i < 3; ++i) portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0));
    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 1), 3.0);
}

TEST(Portfolio, SymbolsAreIndependent) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    portfolio.apply_fill(make_fill(2, qp::Side::Sell, 1.0));

    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 1), 2.0);
    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 2), -1.0);
}

TEST(Portfolio, SameSymbolOnDifferentVenuesIsIndependent) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0, 1, 0, 0.0, kUsdm));
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 1.0, 100.0, 2, 0, 0.0, kCoinm));

    EXPECT_EQ(portfolio.position(kExchange, kUsdm, 1), 2.0);
    EXPECT_EQ(portfolio.position(kExchange, kCoinm, 1), -1.0);
}

TEST(Portfolio, BuyingCostsCashPlusFee) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0, 1, 0, 1.5));
    EXPECT_EQ(portfolio.cash(), -201.5);
}

TEST(Portfolio, SellingCreditsCashMinusFee) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, 100.0, 1, 0, 1.5));
    EXPECT_EQ(portfolio.cash(), 198.5);
}

TEST(Portfolio, PositiveFundingRateDebitsALongPosition) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    portfolio.apply_mark_price(make_mark_price_kline(1, 0, 100.0));

    portfolio.apply_funding(make_funding(1, 0, 0.0001));
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0 - 0.02);
}

TEST(Portfolio, PositiveFundingRateCreditsAShortPosition) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0));
    portfolio.apply_mark_price(make_mark_price_kline(1, 0, 100.0));

    portfolio.apply_funding(make_funding(1, 0, 0.0001));
    EXPECT_DOUBLE_EQ(portfolio.cash(), 200.0 + 0.02);
}

TEST(Portfolio, FundingIsANoOpUntilAMarkPriceHasBeenSet) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));

    portfolio.apply_funding(make_funding(1, 0, 0.0001));
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0);
}

TEST(Portfolio, FundingHasNoEffectOnAFlatPosition) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_funding(make_funding(1, 0, 0.0001));
    EXPECT_EQ(portfolio.cash(), 0.0);
}

TEST(Portfolio, EquityIsCashUntilAnyPriceIsMarked) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0));
    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());
}

TEST(Portfolio, EquityAddsMarkToMarketValueOfHeldPositions) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0));
    portfolio.apply_mark_price(make_trade(1, 0, 110.0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), -200.0 + 2.0 * 110.0);
}

TEST(Portfolio, EquityAddsMarkToMarketValueFromMarkPriceKlineToo) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, 100.0));
    portfolio.apply_mark_price(make_mark_price_kline(1, 0, 90.0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), 200.0 + (-2.0) * 90.0);
}

TEST(Portfolio, ApplyMarkPriceIgnoresFundingEvents) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0));
    portfolio.apply_mark_price(make_funding(1, 0, 0.0001));

    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());
}

TEST(Portfolio, ApplyMarkPriceIgnoresBookDiffEvents) {
    Portfolio portfolio{make_subscription()};
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, 100.0));
    portfolio.apply_mark_price(make_book_diff(1, 0, 0, 0, 0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());
}
