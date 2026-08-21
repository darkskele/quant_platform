#include <gtest/gtest.h>

#include "portfolio.hpp"
#include "support/fill_builders.hpp"
#include "types.hpp"

using qp::test::make_fill;

TEST(Portfolio, FlatUntilAnyFillArrives) {
    qp::Portfolio portfolio;
    EXPECT_EQ(portfolio.position(1), 0.0);
    EXPECT_EQ(portfolio.view().position(1), 0.0);
}

TEST(Portfolio, BuyIncreasesSellDecreasesPosition) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    EXPECT_EQ(portfolio.position(1), 2.0);

    portfolio.apply_fill(make_fill(1, qp::Side::Sell, 0.5));
    EXPECT_EQ(portfolio.position(1), 1.5);
}

TEST(Portfolio, RepeatedFillsAccumulate) {
    qp::Portfolio portfolio;
    for (int i = 0; i < 3; ++i) portfolio.apply_fill(make_fill(1, qp::Side::Buy, 1.0));
    EXPECT_EQ(portfolio.position(1), 3.0);
}

TEST(Portfolio, SymbolsAreIndependent) {
    qp::Portfolio portfolio;
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 2.0));
    portfolio.apply_fill(make_fill(2, qp::Side::Sell, 1.0));

    EXPECT_EQ(portfolio.position(1), 2.0);
    EXPECT_EQ(portfolio.position(2), -1.0);
}

TEST(Portfolio, StateViewReflectsFillsAppliedAfterConstruction) {
    qp::Portfolio portfolio;
    qp::StateView view = portfolio.view();  // non-owning — live window, not a snapshot

    EXPECT_EQ(view.position(1), 0.0);
    portfolio.apply_fill(make_fill(1, qp::Side::Buy, 5.0));
    EXPECT_EQ(view.position(1), 5.0);
}
