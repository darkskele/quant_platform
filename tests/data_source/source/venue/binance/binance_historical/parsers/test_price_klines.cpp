#include <gtest/gtest.h>

#include <string_view>

#include "mark_klines.hpp"
#include "premium_klines.hpp"
#include "types.hpp"

using qp::MarkPriceKlineEvent;
using qp::PremiumIndexKlineEvent;
using qp::Timestamp;
using qp::data_source::source::venue::binance::parsers::parse_mark_klines_row;
using qp::data_source::source::venue::binance::parsers::parse_premium_klines_row;

namespace {

// Rows captured from live files.
constexpr std::string_view kMarkRow =
    "1748822400000,105589.61297464,105683.60000000,105362.40000000,105378.90000000,0,1748825999999,"
    "0,3600,0,0,0";

constexpr std::string_view kPremiumRow =
    "1748822400000,-0.00057025,0,-0.00080144,-0.00049096,0,1748825999999,0,720,0,0,0";

}  // namespace

TEST(BinanceMarkKlineParser, ParsesRow) {
    Timestamp           ts{};
    MarkPriceKlineEvent mark{};
    ASSERT_TRUE(parse_mark_klines_row(kMarkRow, ts, mark));

    EXPECT_EQ(ts, 1748822400000LL * 1'000'000);
    EXPECT_EQ(mark.close_time, 1748825999999LL * 1'000'000);
    EXPECT_DOUBLE_EQ(mark.open, 105589.61297464);
    EXPECT_DOUBLE_EQ(mark.high, 105683.60);
    EXPECT_DOUBLE_EQ(mark.low, 105362.40);
    EXPECT_DOUBLE_EQ(mark.close, 105378.90);
}

TEST(BinanceMarkKlineParser, RejectsTruncatedRow) {
    Timestamp           ts{};
    MarkPriceKlineEvent mark{};
    EXPECT_FALSE(parse_mark_klines_row("1748822400000,105589.61297464", ts, mark));
}

// Premium index values are rates, so negatives are the normal case.
TEST(BinancePremiumKlineParser, ParsesNegativeRates) {
    Timestamp              ts{};
    PremiumIndexKlineEvent premium{};
    ASSERT_TRUE(parse_premium_klines_row(kPremiumRow, ts, premium));

    EXPECT_EQ(ts, 1748822400000LL * 1'000'000);
    EXPECT_EQ(premium.close_time, 1748825999999LL * 1'000'000);
    EXPECT_DOUBLE_EQ(premium.open, -0.00057025);
    EXPECT_DOUBLE_EQ(premium.high, 0.0);
    EXPECT_DOUBLE_EQ(premium.low, -0.00080144);
    EXPECT_DOUBLE_EQ(premium.close, -0.00049096);
}

TEST(BinancePremiumKlineParser, FailureLeavesOutputUntouched) {
    Timestamp              ts{42};
    PremiumIndexKlineEvent premium{.close_time = 7, .open = 1.0};
    EXPECT_FALSE(parse_premium_klines_row("1748822400000,-0.00057025,nope", ts, premium));

    EXPECT_EQ(ts, 42);
    EXPECT_EQ(premium.close_time, 7);
    EXPECT_DOUBLE_EQ(premium.open, 1.0);
}
