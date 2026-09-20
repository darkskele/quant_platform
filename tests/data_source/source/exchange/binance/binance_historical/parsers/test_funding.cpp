#include <gtest/gtest.h>

#include <string_view>

#include "funding.hpp"
#include "types.hpp"

using qp::FundingEvent;
using qp::Timestamp;
using qp::data_source::source::exchange::binance::parsers::is_header_row;
using qp::data_source::source::exchange::binance::parsers::parse_funding_row;

namespace {

// Rows captured from live files.
constexpr std::string_view kHeader       = "calc_time,funding_interval_hours,last_funding_rate";
constexpr std::string_view kEightHourRow = "1748736000001,8,-0.00000582";
constexpr std::string_view kFourHourRow  = "1748750400000,4,0.00010000";
constexpr std::string_view kOneHourRow   = "1748739600000,1,0.00002500";

}  // namespace

TEST(BinanceFundingParser, ParsesRow) {
    Timestamp    ts{};
    FundingEvent funding{};
    ASSERT_TRUE(parse_funding_row(kEightHourRow, ts, funding));

    EXPECT_EQ(ts, 1748736000001LL * 1'000'000);
    EXPECT_DOUBLE_EQ(funding.funding_rate, -0.00000582);
    EXPECT_EQ(funding.interval_hours, 8);
}

// Interval varies by symbol, so an 8h rate and a 1h rate are not comparable.
TEST(BinanceFundingParser, CapturesNonDefaultIntervals) {
    Timestamp    ts{};
    FundingEvent funding{};

    ASSERT_TRUE(parse_funding_row(kFourHourRow, ts, funding));
    EXPECT_EQ(funding.interval_hours, 4);

    ASSERT_TRUE(parse_funding_row(kOneHourRow, ts, funding));
    EXPECT_EQ(funding.interval_hours, 1);
    EXPECT_DOUBLE_EQ(funding.funding_rate, 0.000025);
}

TEST(BinanceFundingParser, DetectsHeaderRow) {
    EXPECT_TRUE(is_header_row(kHeader));
    EXPECT_FALSE(is_header_row(kEightHourRow));
}

TEST(BinanceFundingParser, RejectsHeaderRow) {
    Timestamp    ts{};
    FundingEvent funding{};
    EXPECT_FALSE(parse_funding_row(kHeader, ts, funding));
}

TEST(BinanceFundingParser, RejectsTruncatedRow) {
    Timestamp    ts{};
    FundingEvent funding{};
    EXPECT_FALSE(parse_funding_row("1748736000001,8", ts, funding));
}

TEST(BinanceFundingParser, RejectsNonPositiveInterval) {
    Timestamp    ts{};
    FundingEvent funding{};
    EXPECT_FALSE(parse_funding_row("1748736000001,0,-0.00000582", ts, funding));
}

TEST(BinanceFundingParser, FailureLeavesOutputUntouched) {
    Timestamp    ts{42};
    FundingEvent funding{.funding_rate = 1.5, .interval_hours = 4};
    EXPECT_FALSE(parse_funding_row("1748736000001,8,nope", ts, funding));

    EXPECT_EQ(ts, 42);
    EXPECT_DOUBLE_EQ(funding.funding_rate, 1.5);
    EXPECT_EQ(funding.interval_hours, 4);
}
