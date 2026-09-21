#include <gtest/gtest.h>

#include <charconv>
#include <string>
#include <string_view>

#include "csv_field.hpp"

using qp::Timestamp;
using qp::data_source::source::exchange::binance::parsers::days_from_civil;
using qp::data_source::source::exchange::binance::parsers::take_bool;
using qp::data_source::source::exchange::binance::parsers::take_datetime;
using qp::data_source::source::exchange::binance::parsers::take_decimal;
using qp::data_source::source::exchange::binance::parsers::take_stamp;
using qp::data_source::source::exchange::binance::parsers::to_nanos;

namespace {

double reference(std::string_view text) {
    double     out{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    EXPECT_EQ(result.ec, std::errc{}) << text;
    return out;
}

double parse_one(std::string_view text) {
    std::string_view row = text;
    double           out{};
    EXPECT_TRUE(take_decimal(row, out)) << text;
    return out;
}

}  // namespace

// The fast path has to agree with the general parser bit for bit, or the
// backtest silently prices off a different number than the file holds.
TEST(CsvField, DecimalMatchesFromChars) {
    constexpr std::string_view values[] = {
        "105583.30",        "105642.93000000", "0",    "3928.600",    "432.43804000",
        "45644388.034643",  "-0.00057025",     "1e-8", "0.00000582",  "26817.93000000",
        "1140408482.84249", "196511690.26110", "1.0",  "-0.00080144",
    };

    for (const auto& value : values) {
        SCOPED_TRACE(value);
        EXPECT_DOUBLE_EQ(parse_one(value), reference(value));
    }
}

TEST(CsvField, DecimalAdvancesPastComma) {
    std::string_view row = "1.5,2.5,3.5";
    double           a{}, b{}, c{};
    ASSERT_TRUE(take_decimal(row, a));
    ASSERT_TRUE(take_decimal(row, b));
    ASSERT_TRUE(take_decimal(row, c));
    EXPECT_DOUBLE_EQ(a, 1.5);
    EXPECT_DOUBLE_EQ(b, 2.5);
    EXPECT_DOUBLE_EQ(c, 3.5);
    EXPECT_TRUE(row.empty());
}

TEST(CsvField, DecimalHandlesNegativeAndSignedForms) {
    EXPECT_DOUBLE_EQ(parse_one("-0.00057025"), -0.00057025);
    EXPECT_DOUBLE_EQ(parse_one("+1.25"), 1.25);
}

// Exponent form is outside the digit scan and falls back.
TEST(CsvField, DecimalFallsBackOnExponent) {
    EXPECT_DOUBLE_EQ(parse_one("1.5e3"), 1500.0);
    EXPECT_DOUBLE_EQ(parse_one("-2.5E-2"), -0.025);
}

// More digits than an int64 mantissa holds, so the fallback takes it.
TEST(CsvField, DecimalFallsBackOnOverlongMantissa) {
    constexpr std::string_view value = "123456789012345678901.5";
    EXPECT_DOUBLE_EQ(parse_one(value), reference(value));
}

TEST(CsvField, DecimalRejectsGarbage) {
    std::string_view row = "nope";
    double           out{};
    EXPECT_FALSE(take_decimal(row, out));
}

TEST(CsvField, DecimalRejectsEmptyField) {
    std::string_view row = "";
    double           out{};
    EXPECT_FALSE(take_decimal(row, out));
}

TEST(CsvField, StampUnitsNormaliseToNanos) {
    EXPECT_EQ(to_nanos(1748822400000LL), 1748822400000000000LL);
    EXPECT_EQ(to_nanos(1748822400000000LL), 1748822400000000000LL);
}

TEST(CsvField, StampAdvancesPastComma) {
    std::string_view row = "1748822400000,1748825999999";
    Timestamp        open{}, close{};
    ASSERT_TRUE(take_stamp(row, open));
    ASSERT_TRUE(take_stamp(row, close));
    EXPECT_EQ(open, 1748822400000LL * 1'000'000);
    EXPECT_EQ(close, 1748825999999LL * 1'000'000);
    EXPECT_TRUE(row.empty());
}

TEST(CsvField, StampRejectsNonPositiveAndOversized) {
    Timestamp        out{};
    std::string_view zero = "0";
    EXPECT_FALSE(take_stamp(zero, out));

    std::string_view huge = "999999999999999999";
    EXPECT_FALSE(take_stamp(huge, out));
}

TEST(CsvField, StampRejectsDecimal) {
    std::string_view row = "1748822400000.5";
    Timestamp        out{};
    EXPECT_FALSE(take_stamp(row, out));
}

// 2025-06-02 00:00:08 UTC, the shape metrics and bookDepth stamp with.
TEST(CsvField, DatetimeParsesKnownInstant) {
    std::string_view row = "2025-06-02 00:00:08";
    Timestamp        out{};
    ASSERT_TRUE(take_datetime(row, out));
    EXPECT_EQ(out, 1748822408LL * 1'000'000'000LL);
    EXPECT_TRUE(row.empty());
}

TEST(CsvField, DatetimeAdvancesPastComma) {
    std::string_view row = "2025-06-02 00:00:10,-5,7708.55000000";
    Timestamp        out{};
    ASSERT_TRUE(take_datetime(row, out));
    EXPECT_EQ(out, 1748822410LL * 1'000'000'000LL);
    EXPECT_EQ(row, "-5,7708.55000000");
}

TEST(CsvField, DatetimeRejectsDateOnly) {
    std::string_view row = "2025-06-02";
    Timestamp        out{};
    EXPECT_FALSE(take_datetime(row, out));
    EXPECT_EQ(row, "2025-06-02");
}

TEST(CsvField, DatetimeRejectsOutOfRangeFields) {
    Timestamp out{};
    for (std::string_view text :
         {"2025-13-02 00:00:00", "2025-00-02 00:00:00", "2025-06-31 00:00:00",
          "2025-06-00 00:00:00", "2025-06-02 24:00:00", "2025-06-02 00:60:00",
          "2025-06-02 00:00:60"}) {
        std::string_view row = text;
        EXPECT_FALSE(take_datetime(row, out)) << text;
    }
}

TEST(CsvField, DatetimeRejectsWrongSeparators) {
    Timestamp out{};
    for (std::string_view text : {"2025/06/02 00:00:00", "2025-06-02T00:00:00",
                                  "2025-06-02 00-00-00", "20250602 00:00:00"}) {
        std::string_view row = text;
        EXPECT_FALSE(take_datetime(row, out)) << text;
    }
}

// An epoch stamp is a different format, not this one with a tail, so it must
// not parse as a truncated datetime.
TEST(CsvField, DatetimeRejectsEpochStamp) {
    std::string_view row = "1748822400000";
    Timestamp        out{};
    EXPECT_FALSE(take_datetime(row, out));
}

TEST(CsvField, DatetimeRejectsLongerFieldWithoutComma) {
    std::string_view row = "2025-06-02 00:00:08.500";
    Timestamp        out{};
    EXPECT_FALSE(take_datetime(row, out));
}

// days_from_civil is now load bearing for take_datetime and for the stream's
// file stamp window, so the boundaries get their own case.
TEST(CsvField, CivilDaysCoverEpochAndLeapBoundaries) {
    EXPECT_EQ(days_from_civil(1970, 1, 1), 0);
    EXPECT_EQ(days_from_civil(2000, 3, 1), 11017);
    EXPECT_EQ(days_from_civil(2024, 2, 29), 19782);
    EXPECT_EQ(days_from_civil(2100, 3, 1), 47541);
}

TEST(CsvField, DatetimeAcceptsLeapDayAndRejectsNonLeap) {
    std::string_view leap = "2024-02-29 12:00:00";
    Timestamp        out{};
    ASSERT_TRUE(take_datetime(leap, out));
    EXPECT_EQ(out, (19782LL * 86'400LL + 12LL * 3'600LL) * 1'000'000'000LL);

    std::string_view not_leap = "2025-02-29 12:00:00";
    EXPECT_FALSE(take_datetime(not_leap, out));

    // 1900 is divisible by 4 but not a leap year.
    std::string_view century = "1900-02-29 12:00:00";
    EXPECT_FALSE(take_datetime(century, out));
}

TEST(CsvField, BoolTakesBothCasesAndAdvances) {
    std::string_view row = "true,False,True,false";
    bool             a{}, b{true}, c{}, d{true};
    ASSERT_TRUE(take_bool(row, a));
    ASSERT_TRUE(take_bool(row, b));
    ASSERT_TRUE(take_bool(row, c));
    ASSERT_TRUE(take_bool(row, d));
    EXPECT_TRUE(a);
    EXPECT_FALSE(b);
    EXPECT_TRUE(c);
    EXPECT_FALSE(d);
    EXPECT_TRUE(row.empty());
}

TEST(CsvField, BoolRejectsAnythingElseAndLeavesRow) {
    bool out{};
    for (std::string_view text : {"TRUE", "1", "", "tru", "falsey"}) {
        std::string_view row = text;
        EXPECT_FALSE(take_bool(row, out)) << text;
        EXPECT_EQ(row, text);
    }
}
