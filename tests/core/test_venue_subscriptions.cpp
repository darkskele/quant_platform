#include <gtest/gtest.h>

#include <array>

#include "venue_subscriptions.hpp"

using qp::venue_consumer_count;
using qp::venue_consumer_index;
using qp::VenueSubscription;

namespace {

// strategy 0: venues {1, 2}; strategy 1: venue {2}; strategy 2: venue {1}.
constexpr std::array<VenueSubscription, 4> kSubs = {{
    {0, 1},
    {0, 2},
    {1, 2},
    {2, 1},
}};

}  // namespace

// Compile-time: the whole point is these are usable as template arguments.
static_assert(venue_consumer_count(kSubs, 1) == 2);
static_assert(venue_consumer_count(kSubs, 2) == 2);
static_assert(venue_consumer_index(kSubs, 0, 1) == 0);
static_assert(venue_consumer_index(kSubs, 2, 1) == 1);
static_assert(venue_consumer_index(kSubs, 0, 2) == 0);
static_assert(venue_consumer_index(kSubs, 1, 2) == 1);

TEST(VenueSubscriptions, ConsumerCountMatchesSubscriberTotal) {
    EXPECT_EQ(venue_consumer_count(kSubs, 1), 2u);
    EXPECT_EQ(venue_consumer_count(kSubs, 2), 2u);
}

TEST(VenueSubscriptions, ConsumerCountIsZeroForAnUnsubscribedVenue) {
    EXPECT_EQ(venue_consumer_count(kSubs, 99), 0u);
}

TEST(VenueSubscriptions, ConsumerIndexIsRankAmongThatVenuesSubscribersInDeclarationOrder) {
    EXPECT_EQ(venue_consumer_index(kSubs, 0, 1), 0u);
    EXPECT_EQ(venue_consumer_index(kSubs, 2, 1), 1u);
    EXPECT_EQ(venue_consumer_index(kSubs, 0, 2), 0u);
    EXPECT_EQ(venue_consumer_index(kSubs, 1, 2), 1u);
}

// No EXPECT_THROW test for "strategy not subscribed to venue" — consteval
// (venue_subscriptions.hpp) means that case is a compile error at the call
// site, not a catchable runtime exception; there's no way to exercise it
// from a passing test binary.
