#include <gtest/gtest.h>

#include "portfolio.hpp"
#include "strategy.hpp"
#include "support/strategy_doubles.hpp"
#include "types.hpp"

using qp::test::NoopStrategy;

static_assert(qp::strategy::Strategy<NoopStrategy>);

TEST(Strategy, NoopStrategyEmitsNoIntents) {
    qp::Portfolio portfolio;
    NoopStrategy  strategy;

    EXPECT_TRUE(strategy.on_event(qp::MarketEvent{}, portfolio.view()).empty());
    EXPECT_TRUE(strategy.on_timer(0, portfolio.view()).empty());
}
