#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include "portfolio.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::test::make_book_diff;
using qp::test::make_fill;
using qp::test::make_funding;
using qp::test::make_mark_price_kline;
using qp::test::make_trade;

namespace {
constexpr std::array<std::size_t, 2> kCounts{3, 3};
using Portfolio = qp::Portfolio<kCounts>;
}  // namespace

TEST(Portfolio, FlatUntilAnyFillArrives) {
    Portfolio portfolio;
    EXPECT_EQ(portfolio.position(1, 0), 0.0);
}

TEST(Portfolio, BuyIncreasesSellDecreasesPosition) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    EXPECT_EQ(portfolio.position(1, 0), 2.0);

    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 0.5));
    EXPECT_EQ(portfolio.position(1, 0), 1.5);
}

TEST(Portfolio, RepeatedFillsAccumulate) {
    Portfolio portfolio;
    for (int i = 0; i < 3; ++i) portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0));
    EXPECT_EQ(portfolio.position(1, 0), 3.0);
}

TEST(Portfolio, SymbolsAreIndependent) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    portfolio.apply_fill(make_fill(2, qp::Side::Sell, 1.0));

    EXPECT_EQ(portfolio.position(1, 0), 2.0);
    EXPECT_EQ(portfolio.position(2, 0), -1.0);
}

// D44: the actual property FundingCarryStrategy depends on — the same
// symbol on two different venues (e.g. spot + perp BTCUSDT) must hold
// independent positions, not collide into one slot.
TEST(Portfolio, SameSymbolOnDifferentVenuesIsIndependent) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0, /*order_id=*/1,
                                   /*ts=*/0, /*fee=*/0.0, /*market=*/0));
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 1.0, /*price=*/100.0, /*order_id=*/2,
                                   /*ts=*/0, /*fee=*/0.0, /*market=*/1));

    EXPECT_EQ(portfolio.position(1, 0), 2.0);
    EXPECT_EQ(portfolio.position(1, 1), -1.0);
}

TEST(Portfolio, BuyingCostsCashPlusFee) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0,
                                   /*fee=*/1.5));
    EXPECT_EQ(portfolio.cash(), -201.5);  // -(2*100) - 1.5
}

TEST(Portfolio, SellingCreditsCashMinusFee) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, /*price=*/100.0, /*order_id=*/1,
                                   /*ts=*/0,
                                   /*fee=*/1.5));
    EXPECT_EQ(portfolio.cash(), 198.5);  // (2*100) - 1.5
}

TEST(Portfolio, PositiveFundingRateDebitsALongPosition) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));  // now long 2
    // apply_funding() settles against funding_mark_price_, which stays 0
    // (a silent no-op) until a MarkPriceKlineEvent sets it.
    portfolio.apply_mark_price(make_mark_price_kline(1, 0, 100.0));

    portfolio.apply_funding(make_funding(1, 0, /*rate=*/0.0001));
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0 - 0.02);  // -(2*100) buy cost, then -2*100*0.0001
}

TEST(Portfolio, PositiveFundingRateCreditsAShortPosition) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0));  // now short 2
    portfolio.apply_mark_price(make_mark_price_kline(1, 0, 100.0));

    portfolio.apply_funding(make_funding(1, 0, /*rate=*/0.0001));
    EXPECT_DOUBLE_EQ(portfolio.cash(), 200.0 + 0.02);  // +(2*100) sell proceeds, then +2*100*0.0001
}

TEST(Portfolio, FundingIsANoOpUntilAMarkPriceHasBeenSet) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));  // now long 2

    portfolio.apply_funding(make_funding(1, 0, /*rate=*/0.0001));  // no mark price seen yet
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0);  // funding settles against 0, not a real price
}

TEST(Portfolio, FundingHasNoEffectOnAFlatPosition) {
    Portfolio portfolio;
    portfolio.apply_funding(make_funding(1, 0, 0.0001));
    EXPECT_EQ(portfolio.cash(), 0.0);
}

// D46: equity() is what a drawdown/kill-switch RiskGate actually needs —
// cash() alone can't tell "bought an asset" from "lost money".
TEST(Portfolio, EquityIsCashUntilAnyPriceIsMarked) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());
}

TEST(Portfolio, EquityAddsMarkToMarketValueOfHeldPositions) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));  // cash -= 200
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/110.0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), -200.0 + 2.0 * 110.0);  // up 20 on paper
}

TEST(Portfolio, EquityAddsMarkToMarketValueFromMarkPriceKlineToo) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, /*price=*/100.0));  // cash += 200
    portfolio.apply_mark_price(make_mark_price_kline(1, 0, /*close=*/90.0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), 200.0 + (-2.0) * 90.0);  // short, price fell: up 20
}

TEST(Portfolio, ApplyMarkPriceIgnoresFundingEvents) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_funding(1, 0, /*rate=*/0.0001));

    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());  // FundingEvent carries no scalar price
}

TEST(Portfolio, ApplyMarkPriceIgnoresBookDiffEvents) {
    Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_book_diff(1, 0, 0, 0, 0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());  // no scalar price on a BookDiff
}
