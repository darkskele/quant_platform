#include <gtest/gtest.h>

#include "clock.hpp"
#include "sim_clock.hpp"

static_assert(qp::Clock<qp::SimClock>);

TEST(SimClock, StartsAtZero) { EXPECT_EQ(qp::SimClock{}.now(), 0); }

TEST(SimClock, AdvanceSetsNow) {
    qp::SimClock clock;
    clock.advance(12345);
    EXPECT_EQ(clock.now(), 12345);
    clock.advance(67890);
    EXPECT_EQ(clock.now(), 67890);
}
