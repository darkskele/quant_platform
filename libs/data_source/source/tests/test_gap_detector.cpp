#include <gtest/gtest.h>

#include "gap_detector.hpp"

using qp::source::SequenceGapDetector;

TEST(GapDetector, FirstEventForSymbolIsNeverAGap) {
    SequenceGapDetector d;
    EXPECT_FALSE(d.check_and_record(/*symbol=*/1, /*prev_seq=*/999, /*seq=*/100).has_value());
}

TEST(GapDetector, ContinuousSequenceIsNotAGap) {
    SequenceGapDetector d;
    d.check_and_record(1, 0, 100);
    EXPECT_FALSE(d.check_and_record(1, 100, 105).has_value());
    EXPECT_FALSE(d.check_and_record(1, 105, 110).has_value());
}

TEST(GapDetector, DiscontinuitySignalsAGapWithDetails) {
    SequenceGapDetector d;
    d.check_and_record(1, 0, 100);
    auto gap = d.check_and_record(1, 150, 200);  // should have continued from 100, got 150
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->expected_prev_seq, 100u);
    EXPECT_EQ(gap->actual_prev_seq, 150u);
}

TEST(GapDetector, SymbolsAreTrackedIndependently) {
    SequenceGapDetector d;
    d.check_and_record(1, 0, 100);
    d.check_and_record(2, 0, 500);
    EXPECT_FALSE(d.check_and_record(1, 100, 105).has_value());
    EXPECT_FALSE(d.check_and_record(2, 500, 505).has_value());

    d.check_and_record(1, 999, 1000);                           // gap on symbol 1
    EXPECT_FALSE(d.check_and_record(2, 505, 510).has_value());  // symbol 2 unaffected
}

TEST(GapDetector, ResetForgetsSymbolState) {
    SequenceGapDetector d;
    d.check_and_record(1, 0, 100);
    d.reset(1);
    // After reset, the next event for symbol 1 is treated as the first ever.
    EXPECT_FALSE(d.check_and_record(1, 999, 105).has_value());
}
