#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "partition.hpp"

using qp::sink::day_key_for;
using qp::sink::DayKey;
using qp::sink::format_day;
using qp::sink::needs_rotation;

namespace {
constexpr std::int64_t kNanosPerDay = 86400LL * 1'000'000'000LL;
}

TEST(Partition, DayKeyForEpochIsZero) { EXPECT_EQ(day_key_for(0), 0); }

TEST(Partition, DayKeyStaysZeroUntilTheNextDayBoundary) {
    EXPECT_EQ(day_key_for(kNanosPerDay - 1), 0);  // one ns before midnight day 1
    EXPECT_EQ(day_key_for(kNanosPerDay), 1);      // exactly midnight day 1
}

TEST(Partition, DayKeyForKnownDate) {
    // 2024-01-01T00:00:00Z = unix ts 1704067200 = epoch day 19723 (well-known
    // reference value — independent of this file's own chrono usage).
    EXPECT_EQ(day_key_for(1704067200LL * 1'000'000'000LL), 19723);
    EXPECT_EQ(day_key_for(1704067200LL * 1'000'000'000LL + kNanosPerDay / 2), 19723);
    EXPECT_EQ(day_key_for(1704067200LL * 1'000'000'000LL + kNanosPerDay), 19724);
}

TEST(Partition, FormatDayMatchesKnownDates) {
    EXPECT_EQ(format_day(0), "1970-01-01");
    EXPECT_EQ(format_day(1), "1970-01-02");
    EXPECT_EQ(format_day(19723), "2024-01-01");
}

TEST(Partition, NeedsRotationWhenNothingOpenYet) { EXPECT_TRUE(needs_rotation(std::nullopt, 100)); }

TEST(Partition, NoRotationOnTheSameDay) { EXPECT_FALSE(needs_rotation(DayKey{100}, 100)); }

TEST(Partition, RotatesOnADifferentDay) {
    EXPECT_TRUE(needs_rotation(DayKey{100}, 101));
    EXPECT_TRUE(needs_rotation(DayKey{100}, 99));  // "in the past" also rotates — no special-casing
}
