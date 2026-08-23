#include <gtest/gtest.h>

#include "resync_policy.hpp"

using qp::source::AlignmentRule;
using qp::source::FuturesAlignment;
using qp::source::SpotAlignment;

static_assert(AlignmentRule<FuturesAlignment>);
static_assert(AlignmentRule<SpotAlignment>);

// The exact D13 boundary: last_update_id lands precisely on a buffered
// event's final seq (u), no slack either side. Futures brackets it (no
// offset); spot's own rule (+1) does not — this is the literal off-by-one
// D13 was.
TEST(AlignmentRule, FuturesBracketsExactUpperBoundary) {
    EXPECT_TRUE(FuturesAlignment::brackets(/*U=*/996, /*last_update_id=*/1000, /*u=*/1000));
}

TEST(AlignmentRule, SpotDoesNotBracketExactUpperBoundaryFuturesWouldAccept) {
    EXPECT_FALSE(SpotAlignment::brackets(/*U=*/996, /*last_update_id=*/1000, /*u=*/1000));
}

TEST(AlignmentRule, SpotBracketsOneBelowTheUpperBoundary) {
    // Spot's +1 slack: last_update_id+1 == u still brackets.
    EXPECT_TRUE(SpotAlignment::brackets(/*U=*/995, /*last_update_id=*/999, /*u=*/1000));
}

TEST(AlignmentRule, BothRejectWhenLastUpdateIdIsBelowTheWholeRange) {
    EXPECT_FALSE(FuturesAlignment::brackets(500, 100, 600));
    EXPECT_FALSE(SpotAlignment::brackets(500, 100, 600));
}
