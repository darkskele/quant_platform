#include <gtest/gtest.h>

#include <charconv>
#include <string>
#include <string_view>

#include "csv_field.hpp"

using qp::Timestamp;
using qp::data_source::source::venue::binance::parsers::take_decimal;
using qp::data_source::source::venue::binance::parsers::take_stamp;
using qp::data_source::source::venue::binance::parsers::to_nanos;

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
