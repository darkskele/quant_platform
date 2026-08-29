#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>

#include "partition.hpp"
#include "support/scratch_dir.hpp"

using qp::test::ScratchDir;
using qp::data_source::wire::day_key_for;
using qp::data_source::wire::DayKey;
using qp::data_source::wire::format_day;
using qp::data_source::wire::list_segments;
using qp::data_source::wire::needs_rotation;
using qp::data_source::wire::segment_path;

namespace {
constexpr std::int64_t kNanosPerDay = 86400LL * 1'000'000'000LL;

void touch(const std::filesystem::path& p) { std::ofstream{p}; }
}  // namespace

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

TEST(Partition, SegmentPathMatchesFileRecorderNamingConvention) {
    auto path = segment_path("/data/BTCUSDT", 19723, 2);
    EXPECT_EQ(path, std::filesystem::path("/data/BTCUSDT/2024-01-01.002.bin.zst"));
}

class ListSegmentsTest : public ::testing::Test {
   protected:
    ScratchDir             scratch_{"qp_wire_partition_test_"};
    std::filesystem::path& dir_ = scratch_.path;
};

TEST_F(ListSegmentsTest, EmptyWhenNoSegmentsExist) {
    EXPECT_TRUE(list_segments(dir_, 19723).empty());
}

TEST_F(ListSegmentsTest, ListsContiguousSegmentsInOrder) {
    touch(segment_path(dir_, 19723, 0));
    touch(segment_path(dir_, 19723, 1));
    touch(segment_path(dir_, 19723, 2));

    auto segments = list_segments(dir_, 19723);
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_EQ(segments[0], segment_path(dir_, 19723, 0));
    EXPECT_EQ(segments[1], segment_path(dir_, 19723, 1));
    EXPECT_EQ(segments[2], segment_path(dir_, 19723, 2));
}

TEST_F(ListSegmentsTest, StopsAtFirstGapEvenIfLaterSegmentsExist) {
    touch(segment_path(dir_, 19723, 0));
    touch(
        segment_path(dir_, 19723, 2));  // seq 1 missing — shouldn't happen, but don't skip past it

    auto segments = list_segments(dir_, 19723);
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0], segment_path(dir_, 19723, 0));
}

TEST_F(ListSegmentsTest, DoesNotCrossDayBoundaries) {
    touch(segment_path(dir_, 19723, 0));
    touch(segment_path(dir_, 19724, 0));

    EXPECT_EQ(list_segments(dir_, 19723).size(), 1u);
    EXPECT_EQ(list_segments(dir_, 19724).size(), 1u);
}
