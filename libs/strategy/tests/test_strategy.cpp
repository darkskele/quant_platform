#include <gtest/gtest.h>

#include <vector>

#include "portfolio.hpp"
#include "strategy.hpp"
#include "types.hpp"

namespace {

struct NoopStrategy {
    std::vector<qp::Intent> on_event(const qp::MarketEvent&, qp::StateView) { return {}; }

    std::vector<qp::Intent> on_timer(qp::Timestamp, qp::StateView) { return {}; }
};

}  // namespace

static_assert(qp::Strategy<NoopStrategy>);

TEST(Strategy, NoopStrategyEmitsNoIntents) {
    qp::Portfolio portfolio;
    NoopStrategy  strategy;

    EXPECT_TRUE(strategy.on_event(qp::MarketEvent{}, portfolio.view()).empty());
    EXPECT_TRUE(strategy.on_timer(0, portfolio.view()).empty());
}
