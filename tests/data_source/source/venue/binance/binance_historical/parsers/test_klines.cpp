#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "klines.hpp"
#include "types.hpp"

using qp::KlineEvent;
using qp::Timestamp;
using qp::data_source::source::venue::binance::parsers::is_header_row;
using qp::data_source::source::venue::binance::parsers::parse_klines_row;

namespace {

// Rows captured from live files.
constexpr std::string_view kUsdMHeader =
    "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,"
    "taker_buy_quote_volume,ignore";

constexpr std::string_view kUsdMRowMillis =
    "1748822400000,105583.30,105700.00,105351.80,105379.10,3928.600,1748825999999,414423568.46600,"
    "97149,1862.868,196511690.26110,0";

constexpr std::string_view kSpotRowMicros =
    "1748822400000000,105642.93000000,105735.42000000,105404.31000000,105427.48000000,432.43804000,"
    "1748825999999999,45644388.03464300,124446,192.02670000,20268734.77003200,0";

constexpr std::string_view kSpotRowMillis =
    "1685664000000,26817.93000000,26824.64000000,26505.00000000,26786.03000000,3258.76190000,"
    "1685667599999,86965474.55695710,65278,1560.36714000,41640397.16243640,0";

}  // namespace

TEST(BinanceKlineParser, ParsesUsdMRow) {
    Timestamp  ts{};
    KlineEvent kline{};
    ASSERT_TRUE(parse_klines_row(kUsdMRowMillis, ts, kline));

    EXPECT_EQ(ts, 1748822400000LL * 1'000'000);
    EXPECT_EQ(kline.close_time, 1748825999999LL * 1'000'000);
    EXPECT_DOUBLE_EQ(kline.open, 105583.30);
    EXPECT_DOUBLE_EQ(kline.high, 105700.00);
    EXPECT_DOUBLE_EQ(kline.low, 105351.80);
    EXPECT_DOUBLE_EQ(kline.close, 105379.10);
    EXPECT_DOUBLE_EQ(kline.volume, 3928.600);
}

TEST(BinanceKlineParser, ParsesSpotMillisRow) {
    Timestamp  ts{};
    KlineEvent kline{};
    ASSERT_TRUE(parse_klines_row(kSpotRowMillis, ts, kline));

    EXPECT_EQ(ts, 1685664000000LL * 1'000'000);
    EXPECT_DOUBLE_EQ(kline.open, 26817.93);
    EXPECT_DOUBLE_EQ(kline.volume, 3258.76190000);
}

// Same instant in both units has to land on the same nanosecond.
TEST(BinanceKlineParser, MicrosecondAndMillisecondStampsAgree) {
    Timestamp  micros_ts{};
    Timestamp  millis_ts{};
    KlineEvent ignored{};
    ASSERT_TRUE(parse_klines_row(kSpotRowMicros, micros_ts, ignored));
    ASSERT_TRUE(parse_klines_row(kUsdMRowMillis, millis_ts, ignored));

    EXPECT_EQ(micros_ts, millis_ts);
    EXPECT_EQ(micros_ts, 1748822400000000000LL);
}

TEST(BinanceKlineParser, MicrosecondRowKeepsFieldsIntact) {
    Timestamp  ts{};
    KlineEvent kline{};
    ASSERT_TRUE(parse_klines_row(kSpotRowMicros, ts, kline));

    EXPECT_EQ(kline.close_time, 1748825999999999LL * 1'000);
    EXPECT_DOUBLE_EQ(kline.open, 105642.93);
    EXPECT_DOUBLE_EQ(kline.close, 105427.48);
}

TEST(BinanceKlineParser, DetectsHeaderRow) {
    EXPECT_TRUE(is_header_row(kUsdMHeader));
    EXPECT_FALSE(is_header_row(kUsdMRowMillis));
    EXPECT_FALSE(is_header_row(kSpotRowMicros));
    EXPECT_TRUE(is_header_row(""));
}

TEST(BinanceKlineParser, RejectsTruncatedRow) {
    Timestamp  ts{};
    KlineEvent kline{};
    EXPECT_FALSE(parse_klines_row("1748822400000,105583.30,105700.00", ts, kline));
}

TEST(BinanceKlineParser, RejectsNonNumericField) {
    Timestamp  ts{};
    KlineEvent kline{};
    EXPECT_FALSE(parse_klines_row(kUsdMHeader, ts, kline));
}

TEST(BinanceKlineParser, RejectsEmptyRow) {
    Timestamp  ts{};
    KlineEvent kline{};
    EXPECT_FALSE(parse_klines_row("", ts, kline));
}

// A failed parse must not half write the caller's event.
TEST(BinanceKlineParser, FailureLeavesOutputUntouched) {
    Timestamp  ts{42};
    KlineEvent kline{.close_time = 7, .open = 1.0, .high = 2.0, .low = 3.0, .close = 4.0};
    EXPECT_FALSE(parse_klines_row("1748822400000,105583.30,nope", ts, kline));

    EXPECT_EQ(ts, 42);
    EXPECT_EQ(kline.close_time, 7);
    EXPECT_DOUBLE_EQ(kline.open, 1.0);
}

TEST(BinanceKlineParser, IgnoresTrailingCarriageReturn) {
    Timestamp   ts{};
    KlineEvent  kline{};
    std::string row(kUsdMRowMillis);
    row += '\r';
    ASSERT_TRUE(parse_klines_row(row, ts, kline));
    EXPECT_EQ(ts, 1748822400000LL * 1'000'000);
}
