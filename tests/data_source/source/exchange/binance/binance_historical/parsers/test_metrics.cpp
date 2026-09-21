#include <gtest/gtest.h>

#include <cmath>
#include <string_view>

#include "metrics.hpp"
#include "types.hpp"

using qp::OpenInterestEvent;
using qp::Timestamp;
using qp::data_source::source::exchange::binance::parsers::parse_metrics_row;

namespace {

// Real rows off the bucket. USD-M populates all four ratios, Coin-M leaves
// three of them empty on every row it publishes.
constexpr std::string_view kUsdMRow =
    "2025-06-02 00:00:00,BTCUSDT,82795.1230000000000000,8742808969.7711000000000000,1.21422318,"
    "1.53311800,1.13511948,0.94273600";

constexpr std::string_view kCoinMRow =
    "2025-06-02 00:00:00,BTCUSD_PERP,23906290.00000000,22626.52258397,,,,0.52745930";

constexpr Timestamp kStamp = 1748822400LL * 1'000'000'000LL;

}  // namespace

TEST(BinanceMetrics, ParsesUsdMRow) {
    Timestamp         ts{};
    OpenInterestEvent out{};
    ASSERT_TRUE(parse_metrics_row(kUsdMRow, ts, out));

    EXPECT_EQ(ts, kStamp);
    EXPECT_DOUBLE_EQ(out.open_interest, 82795.123);
    EXPECT_DOUBLE_EQ(out.open_interest_value, 8742808969.7711);
    EXPECT_DOUBLE_EQ(out.toptrader_account_ratio, 1.21422318);
    EXPECT_DOUBLE_EQ(out.toptrader_position_ratio, 1.533118);
    EXPECT_DOUBLE_EQ(out.account_long_short_ratio, 1.13511948);
    EXPECT_DOUBLE_EQ(out.taker_long_short_volume_ratio, 0.942736);
}

// Rejecting on a blank ratio would drop every Coin-M row published.
TEST(BinanceMetrics, CoinMBlankRatiosParseAsNaN) {
    Timestamp         ts{};
    OpenInterestEvent out{};
    ASSERT_TRUE(parse_metrics_row(kCoinMRow, ts, out));

    EXPECT_EQ(ts, kStamp);
    EXPECT_DOUBLE_EQ(out.open_interest, 23906290.0);
    EXPECT_DOUBLE_EQ(out.open_interest_value, 22626.52258397);
    EXPECT_TRUE(std::isnan(out.toptrader_account_ratio));
    EXPECT_TRUE(std::isnan(out.toptrader_position_ratio));
    EXPECT_TRUE(std::isnan(out.account_long_short_ratio));
    EXPECT_DOUBLE_EQ(out.taker_long_short_volume_ratio, 0.5274593);
}

TEST(BinanceMetrics, RejectsHeaderRow) {
    constexpr std::string_view header =
        "create_time,symbol,sum_open_interest,sum_open_interest_value,"
        "count_toptrader_long_short_ratio,sum_toptrader_long_short_ratio,count_long_short_ratio,"
        "sum_taker_long_short_vol_ratio";
    Timestamp         ts{};
    OpenInterestEvent out{};
    EXPECT_FALSE(parse_metrics_row(header, ts, out));
}

TEST(BinanceMetrics, RejectsBlankAndShortRows) {
    Timestamp         ts{};
    OpenInterestEvent out{};
    for (std::string_view row : {"", "2025-06-02 00:00:00", "2025-06-02 00:00:00,BTCUSDT",
                                 "2025-06-02 00:00:00,BTCUSDT,82795.123"}) {
        EXPECT_FALSE(parse_metrics_row(row, ts, out)) << row;
    }
}

// Open interest is why this dataset is read, so a blank there is a bad row even
// though a blank ratio is not.
TEST(BinanceMetrics, RejectsBlankOpenInterest) {
    constexpr std::string_view row = "2025-06-02 00:00:00,BTCUSDT,,,1.0,1.0,1.0,1.0";
    Timestamp                  ts{};
    OpenInterestEvent          out{};
    EXPECT_FALSE(parse_metrics_row(row, ts, out));
}

TEST(BinanceMetrics, RejectsMissingSymbol) {
    constexpr std::string_view row = "2025-06-02 00:00:00,,82795.123,8742808969.77,1.0,1.0,1.0,1.0";
    Timestamp                  ts{};
    OpenInterestEvent          out{};
    EXPECT_FALSE(parse_metrics_row(row, ts, out));
}

TEST(BinanceMetrics, RejectsEpochStampRow) {
    constexpr std::string_view row =
        "1748822400000,BTCUSDT,82795.123,8742808969.77,1.0,1.0,1.0,1.0";
    Timestamp         ts{};
    OpenInterestEvent out{};
    EXPECT_FALSE(parse_metrics_row(row, ts, out));
}

// A delisted or brand new contract reports zero, which is data and not a fault.
TEST(BinanceMetrics, ZeroOpenInterestParses) {
    constexpr std::string_view row =
        "2025-06-02 00:00:00,DEADUSDT,0.00000000,0.00000000,0.00000000,0.00000000,0.00000000,"
        "0.00000000";
    Timestamp         ts{};
    OpenInterestEvent out{};
    ASSERT_TRUE(parse_metrics_row(row, ts, out));
    EXPECT_DOUBLE_EQ(out.open_interest, 0.0);
    EXPECT_DOUBLE_EQ(out.taker_long_short_volume_ratio, 0.0);
}

TEST(BinanceMetrics, FailureLeavesOutputUntouched) {
    Timestamp                  ts  = 42;
    OpenInterestEvent          out = {.open_interest = 7.0};
    constexpr std::string_view bad = "not,a,metrics,row,at,all,really,no";
    EXPECT_FALSE(parse_metrics_row(bad, ts, out));
    EXPECT_EQ(ts, 42);
    EXPECT_DOUBLE_EQ(out.open_interest, 7.0);
}
