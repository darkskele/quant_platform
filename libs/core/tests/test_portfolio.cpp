#include <gtest/gtest.h>

#include "portfolio.hpp"
#include "support/fill_builders.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::test::make_book_diff;
using qp::test::make_fill;
using qp::test::make_funding;
using qp::test::make_trade;

TEST(Portfolio, FlatUntilAnyFillArrives) {
    qp::Portfolio portfolio;
    EXPECT_EQ(portfolio.position(1, 0), 0.0);
    EXPECT_EQ(portfolio.view().position(1, 0), 0.0);
}

TEST(Portfolio, BuyIncreasesSellDecreasesPosition) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    EXPECT_EQ(portfolio.position(1, 0), 2.0);

    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 0.5));
    EXPECT_EQ(portfolio.position(1, 0), 1.5);
}

TEST(Portfolio, RepeatedFillsAccumulate) {
    qp::Portfolio portfolio;
    for (int i = 0; i < 3; ++i) portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0));
    EXPECT_EQ(portfolio.position(1, 0), 3.0);
}

TEST(Portfolio, SymbolsAreIndependent) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    portfolio.apply_fill(make_fill(2, qp::Side::Sell, 1.0));

    EXPECT_EQ(portfolio.position(1, 0), 2.0);
    EXPECT_EQ(portfolio.position(2, 0), -1.0);
}

// D44: the actual property FundingCarryStrategy depends on — the same
// symbol on two different venues (e.g. spot + perp BTCUSDT) must hold
// independent positions, not collide into one slot.
TEST(Portfolio, SameSymbolOnDifferentVenuesIsIndependent) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0, /*order_id=*/1,
                                   /*ts=*/0, /*fee=*/0.0, /*venue=*/0));
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 1.0, /*price=*/100.0, /*order_id=*/2,
                                   /*ts=*/0, /*fee=*/0.0, /*venue=*/1));

    EXPECT_EQ(portfolio.position(1, 0), 2.0);
    EXPECT_EQ(portfolio.position(1, 1), -1.0);
}

TEST(Portfolio, StateViewReflectsFillsAppliedAfterConstruction) {
    qp::Portfolio portfolio;
    qp::StateView view = portfolio.view();  // non-owning — live window, not a snapshot

    EXPECT_EQ(view.position(1, 0), 0.0);
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 5.0));
    EXPECT_EQ(view.position(1, 0), 5.0);
}

TEST(Portfolio, BuyingCostsCashPlusFee) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0, /*order_id=*/1, /*ts=*/0,
                                   /*fee=*/1.5));
    EXPECT_EQ(portfolio.cash(), -201.5);  // -(2*100) - 1.5
}

TEST(Portfolio, SellingCreditsCashMinusFee) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, /*price=*/100.0, /*order_id=*/1,
                                   /*ts=*/0,
                                   /*fee=*/1.5));
    EXPECT_EQ(portfolio.cash(), 198.5);  // (2*100) - 1.5
}

TEST(Portfolio, PositiveFundingRateDebitsALongPosition) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));  // now long 2

    portfolio.apply_funding(make_funding(1, 0, /*rate=*/0.0001, /*mark_price=*/100.0));
    EXPECT_DOUBLE_EQ(portfolio.cash(), -200.0 - 0.02);  // -(2*100) buy cost, then -2*100*0.0001
}

TEST(Portfolio, PositiveFundingRateCreditsAShortPosition) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0));  // now short 2

    portfolio.apply_funding(make_funding(1, 0, /*rate=*/0.0001, /*mark_price=*/100.0));
    EXPECT_DOUBLE_EQ(portfolio.cash(), 200.0 + 0.02);  // +(2*100) sell proceeds, then +2*100*0.0001
}

TEST(Portfolio, FundingHasNoEffectOnAFlatPosition) {
    qp::Portfolio portfolio;
    portfolio.apply_funding(make_funding(1, 0, 0.0001, 100.0));
    EXPECT_EQ(portfolio.cash(), 0.0);
}

// D46: equity() is what a drawdown/kill-switch RiskGate actually needs —
// cash() alone can't tell "bought an asset" from "lost money".
TEST(Portfolio, EquityIsCashUntilAnyPriceIsMarked) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());
}

TEST(Portfolio, EquityAddsMarkToMarketValueOfHeldPositions) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));  // cash -= 200
    portfolio.apply_mark_price(make_trade(1, 0, /*price=*/110.0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), -200.0 + 2.0 * 110.0);  // up 20 on paper
}

TEST(Portfolio, EquityMarksFromFundingEventsMarkPriceToo) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 2.0, /*price=*/100.0));  // cash += 200
    portfolio.apply_mark_price(make_funding(1, 0, /*rate=*/0.0, /*mark_price=*/90.0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), 200.0 + (-2.0) * 90.0);  // short, price fell: up 20
}

TEST(Portfolio, ApplyMarkPriceIgnoresBookDiffEvents) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0, /*price=*/100.0));
    portfolio.apply_mark_price(make_book_diff(1, 0, 0, 0, 0));

    EXPECT_DOUBLE_EQ(portfolio.equity(), portfolio.cash());  // no scalar price on a BookDiff
}
