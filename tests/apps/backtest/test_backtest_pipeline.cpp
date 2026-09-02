#include <gtest/gtest.h>

#include <cmath>

#include "funding_carry/funding_carry_backtest.hpp"

// Runs the real FundingCarryBacktest::run() pipeline against the committed
// tests/apps/backtest/fixtures/ dataset (7 days, 1-minute bars, 21
// funding events cycling across FundingCarryStrategy's enter/hold/exit
// thresholds).
TEST(BacktestPipeline, RunsTheRealEntryPointAgainstAWeekOfFixtureDataWithoutViolatingInvariants) {
    qp::backtest::funding_carry::FundingCarryBacktest backtest;
    auto                                              results = backtest.run();

    // Finite, not NaN/inf — a real run that actually touched the book,
    // not a degenerate one.
    EXPECT_TRUE(std::isfinite(results.final_cash));
    EXPECT_TRUE(std::isfinite(results.final_equity));
    EXPECT_TRUE(std::isfinite(results.final_spot_position));
    EXPECT_TRUE(std::isfinite(results.final_futures_position));

    // A week of accumulated fees and funding payments leaves cash moved
    // from zero regardless of where the position ends up.
    EXPECT_NE(results.final_cash, 0.0);

    // Delta-neutral: spot long, futures short, sized identically — a
    // violated invariant means the risk gate's clamp or the strategy's
    // leg-sizing is broken.
    EXPECT_NEAR(results.final_spot_position, -results.final_futures_position, 1e-9);

    // BasicRiskGateConfig's default max_position_qty (10.0) bounds both
    // legs even after the funding cycle's repeated enter/exit churn.
    EXPECT_LE(std::abs(results.final_spot_position), 10.0);
    EXPECT_LE(std::abs(results.final_futures_position), 10.0);
}
