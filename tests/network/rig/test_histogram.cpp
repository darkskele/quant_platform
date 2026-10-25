#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "histogram.hpp"

using qp::network::rig::Histogram;
using qp::network::rig::LogLinearHistogram;

TEST(Histogram, EmptyReadsZero) {
    const Histogram h;
    EXPECT_EQ(h.count(), 0u);
    EXPECT_EQ(h.percentile(50), 0);
    EXPECT_EQ(h.min(), 0);
    EXPECT_EQ(h.max(), 0);
}

namespace {

template <class H>
void expect_tiled() {
    EXPECT_EQ(H::lower(0), 0);
    for (std::size_t i = 0; i + 1 < H::BUCKETS; ++i)
        ASSERT_EQ(H::upper(i) + 1, H::lower(i + 1)) << "bucket " << i;
}

// Every value up to the last bucket sits inside its bucket, at most 1 / PER_GROUP of it wide.
template <class H>
void expect_precise() {
    // Every value while small, then steps of about one percent.
    const auto next = [](std::int64_t v) { return v < 4096 ? v + 1 : v + v / 97; };
    for (std::int64_t v = 0; v <= H::upper(H::BUCKETS - 1); v = next(v)) {
        const auto i = H::index(v);
        ASSERT_LE(H::lower(i), v) << v;
        ASSERT_GE(H::upper(i), v) << v;
        ASSERT_LE(H::upper(i) - H::lower(i), v / H::PER_GROUP) << v;
    }
}

}  // namespace

TEST(Histogram, BucketsTileTheRangeWithNoGap) {
    expect_tiled<Histogram>();
    expect_tiled<LogLinearHistogram<4, 20>>();
}

TEST(Histogram, EveryValueLandsInsideItsBucketWithinItsPrecision) {
    expect_precise<Histogram>();
    expect_precise<LogLinearHistogram<4, 20>>();
}

TEST(Histogram, BucketCountFollowsTheLayout) {
    EXPECT_EQ(Histogram::BUCKETS, 34u * 128);
    EXPECT_EQ((LogLinearHistogram<4, 20>::BUCKETS), 17u * 16);
}

TEST(Histogram, SmallValuesAreExact) {
    Histogram h;
    for (std::int64_t v = 0; v < 100; ++v) h.record(v);
    EXPECT_EQ(h.percentile(50), 49);
    EXPECT_EQ(h.percentile(100), 99);
    EXPECT_EQ(h.min(), 0);
    EXPECT_EQ(h.max(), 99);
    EXPECT_DOUBLE_EQ(h.mean(), 49.5);
}

TEST(Histogram, LargePercentilesAreWithinBucketPrecision) {
    Histogram h;
    for (std::int64_t v = 1; v <= 1'000'000; ++v) h.record(v);
    for (const auto& [p, want] : {std::pair<double, std::int64_t>{50.0, 500'000},
                                  std::pair<double, std::int64_t>{99.0, 990'000},
                                  std::pair<double, std::int64_t>{99.9, 999'000}}) {
        const auto got = h.percentile(p);
        EXPECT_GE(got, want) << p;
        EXPECT_LE(got, want + want / 128) << p;
    }
    EXPECT_EQ(h.percentile(100), 1'000'000);
}

TEST(Histogram, NegativeIsCountedApart) {
    Histogram h;
    h.record(-5);
    h.record(10);
    EXPECT_EQ(h.negative(), 1u);
    EXPECT_EQ(h.count(), 1u);
    EXPECT_EQ(h.min(), 10);
}

TEST(Histogram, HugeValuesShareTheTopBucketAndKeepTheirMax) {
    Histogram          h;
    const std::int64_t huge = std::int64_t{1} << 50;
    h.record(huge);
    EXPECT_EQ(Histogram::index(huge), Histogram::BUCKETS - 1);
    EXPECT_EQ(h.max(), huge);
}
