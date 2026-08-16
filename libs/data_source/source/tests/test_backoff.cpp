#include <gtest/gtest.h>

#include "backoff.hpp"

using qp::source::ExponentialBackoff;
using namespace std::chrono_literals;

TEST(ExponentialBackoff, StartsAtInitialDelay) {
    ExponentialBackoff b(100ms, 10000ms);
    EXPECT_EQ(b.next(), 100ms);
}

TEST(ExponentialBackoff, DoublesEachCall) {
    ExponentialBackoff b(100ms, 10000ms);
    EXPECT_EQ(b.next(), 100ms);
    EXPECT_EQ(b.next(), 200ms);
    EXPECT_EQ(b.next(), 400ms);
    EXPECT_EQ(b.next(), 800ms);
}

TEST(ExponentialBackoff, CapsAtMax) {
    ExponentialBackoff b(1000ms, 3000ms);
    EXPECT_EQ(b.next(), 1000ms);
    EXPECT_EQ(b.next(), 2000ms);
    EXPECT_EQ(b.next(), 3000ms);  // would be 4000, capped to 3000
    EXPECT_EQ(b.next(), 3000ms);  // stays capped
}

TEST(ExponentialBackoff, ResetReturnsToInitial) {
    ExponentialBackoff b(100ms, 10000ms);
    b.next();
    b.next();
    b.next();
    b.reset();
    EXPECT_EQ(b.next(), 100ms);
}
