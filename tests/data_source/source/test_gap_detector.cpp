#include <gtest/gtest.h>

#include "gap_detector.hpp"
#include "resync_policy.hpp"

using qp::source::FuturesAlignment;
using qp::source::SequenceGapDetector;
using qp::source::SpotAlignment;

TEST(GapDetector, FirstEventForSymbolIsNeverAGap) {
    SequenceGapDetector d;
    EXPECT_FALSE(d.check_and_record<FuturesAlignment>(/*symbol=*/1, /*first_seq=*/0,
                                                      /*prev_seq=*/999, /*seq=*/100)
                     .has_value());
}

TEST(GapDetector, ContinuousSequenceIsNotAGap) {
    SequenceGapDetector d;
    d.check_and_record<FuturesAlignment>(1, 0, 0, 100);
    EXPECT_FALSE(d.check_and_record<FuturesAlignment>(1, 0, 100, 105).has_value());
    EXPECT_FALSE(d.check_and_record<FuturesAlignment>(1, 0, 105, 110).has_value());
}

TEST(GapDetector, DiscontinuitySignalsAGapWithDetails) {
    SequenceGapDetector d;
    d.check_and_record<FuturesAlignment>(1, 0, 0, 100);
    // Should have continued (pu == 100), got pu == 150.
    auto gap = d.check_and_record<FuturesAlignment>(1, 0, 150, 200);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->last_seq, 100u);
    EXPECT_EQ(gap->prev_seq, 150u);
}

TEST(GapDetector, SymbolsAreTrackedIndependently) {
    SequenceGapDetector d;
    d.check_and_record<FuturesAlignment>(1, 0, 0, 100);
    d.check_and_record<FuturesAlignment>(2, 0, 0, 500);
    EXPECT_FALSE(d.check_and_record<FuturesAlignment>(1, 0, 100, 105).has_value());
    EXPECT_FALSE(d.check_and_record<FuturesAlignment>(2, 0, 500, 505).has_value());

    d.check_and_record<FuturesAlignment>(1, 0, 999, 1000);  // gap on symbol 1
    EXPECT_FALSE(
        d.check_and_record<FuturesAlignment>(2, 0, 505, 510).has_value());  // symbol 2 unaffected
}

TEST(GapDetector, ResetForgetsSymbolState) {
    SequenceGapDetector d;
    d.check_and_record<FuturesAlignment>(1, 0, 0, 100);
    d.reset(1);
    // After reset, the next event for symbol 1 is treated as the first ever.
    EXPECT_FALSE(d.check_and_record<FuturesAlignment>(1, 0, 999, 105).has_value());
}

// D40: spot's depthUpdate carries no `pu` at all — prev_seq is always 0 on
// a real spot stream. Under FuturesAlignment (the old, only, prev_seq-based
// check) that would false-positive a gap on every message after the first;
// SpotAlignment correctly reads continuity off first_seq (U) chaining
// instead and stays quiet.
TEST(GapDetector, SpotStreamWithNoPuWouldFalsePositiveUnderFuturesRuleButNotSpotRule) {
    SequenceGapDetector futures_detector;
    futures_detector.check_and_record<FuturesAlignment>(1, /*first_seq=*/900, /*prev_seq=*/0,
                                                        /*seq=*/1000);
    auto futures_gap = futures_detector.check_and_record<FuturesAlignment>(
        1, /*first_seq=*/1001, /*prev_seq=*/0, /*seq=*/1100);
    EXPECT_TRUE(futures_gap.has_value());  // false positive: pu (0) != last seq (1000)

    SequenceGapDetector spot_detector;
    spot_detector.check_and_record<SpotAlignment>(1, /*first_seq=*/900, /*prev_seq=*/0,
                                                  /*seq=*/1000);
    auto spot_gap = spot_detector.check_and_record<SpotAlignment>(1, /*first_seq=*/1001,
                                                                  /*prev_seq=*/0, /*seq=*/1100);
    EXPECT_FALSE(spot_gap.has_value());  // correct: U (1001) == last seq (1000) + 1
}

TEST(GapDetector, SpotRuleDetectsARealGapToo) {
    SequenceGapDetector d;
    d.check_and_record<SpotAlignment>(1, /*first_seq=*/900, /*prev_seq=*/0, /*seq=*/1000);
    // A real hole: next diff's U should be 1001, arrives as 1050.
    auto gap =
        d.check_and_record<SpotAlignment>(1, /*first_seq=*/1050, /*prev_seq=*/0, /*seq=*/1100);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->last_seq, 1000u);
    EXPECT_EQ(gap->first_seq, 1050u);
}
