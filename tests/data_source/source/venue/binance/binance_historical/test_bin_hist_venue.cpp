#include <gtest/gtest.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

#include "bin_hist_venue.hpp"

using qp::EventKind;
using qp::FundingEvent;
using qp::KlineEvent;
using qp::MarkPriceKlineEvent;
using qp::data_source::source::venue::binance::binance_historical::BinHistSymbolTable;
using qp::data_source::source::venue::binance::binance_historical::BinHistVenue;

namespace {

std::span<const std::byte> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

// Real captured lines, post-tagging (the SYMBOL,KIND, prefix
// BinHistParserPolicy::parse() expects) — pulled from the same downloaded
// data/binance/BTCUSDT files as bench_bin_hist_parser_policy.cpp.
constexpr std::string_view kRealKlineLine =
    "BTCUSDT,K,1717200000000,67577.90,67680.70,67572.00,67680.40,464.748,1717200299999,"
    "31428593.42840,6933,289.996,19611103.65640,0";

constexpr std::string_view kRealMarkLine =
    "BTCUSDT,M,1717200000000,67570.93117730,67680.70000000,67570.93117730,67678.20000000,0,"
    "1717200299999,0,300,0,0,0";

constexpr std::string_view kRealFundingLine = "BTCUSDT,F,1577836800000,8,-0.00012359";

}  // namespace

TEST(BinHistVenue, ParsesARealFundingEntry) {
    auto ev = BinHistVenue::parse(as_bytes(kRealFundingLine));
    ASSERT_TRUE(ev.has_value());
    ASSERT_TRUE(std::holds_alternative<FundingEvent>(*ev));
    const auto& funding = std::get<FundingEvent>(*ev);
    EXPECT_EQ(funding.base.symbol, *BinHistSymbolTable::id_of("BTCUSDT"));
    EXPECT_DOUBLE_EQ(funding.funding_rate, -0.00012359);
    EXPECT_EQ(funding.base.ts, 1577836800000LL * 1'000'000);  // ms -> ns
}

TEST(BinHistVenue, RejectsAFundingEntryForAnUnknownSymbol) {
    constexpr std::string_view kUnknown = "NOTASYMBOL,F,1577836800000,8,-0.00012359";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kUnknown)).has_value());
}

TEST(BinHistVenue, RejectsAFundingEntryMissingARequiredField) {
    constexpr std::string_view kMissingRate = "BTCUSDT,F,1577836800000,8";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kMissingRate)).has_value());
}

TEST(BinHistVenue, RejectsAnUnrecognizedKindTag) {
    constexpr std::string_view kBadKind = "BTCUSDT,X,1577836800000,8,-0.00012359";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kBadKind)).has_value());
}

TEST(BinHistVenue, RejectsALineWithNoSymbolDelimiter) {
    constexpr std::string_view kNoComma = "BTCUSDT";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kNoComma)).has_value());
}

TEST(BinHistVenue, RejectsALineWithNoKindDelimiter) {
    constexpr std::string_view kNoKind = "BTCUSDT,K";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kNoKind)).has_value());
}

TEST(BinHistVenue, RejectsEmptyInput) {
    EXPECT_FALSE(BinHistVenue::parse(as_bytes("")).has_value());
    EXPECT_FALSE(BinHistVenue::parse(as_bytes("   \n")).has_value());
}

TEST(BinHistVenue, ParsesARealKlineRow) {
    auto ev = BinHistVenue::parse(as_bytes(kRealKlineLine));
    ASSERT_TRUE(ev.has_value());
    ASSERT_TRUE(std::holds_alternative<KlineEvent>(*ev));
    const auto& kline = std::get<KlineEvent>(*ev);
    EXPECT_EQ(kline.base.symbol, *BinHistSymbolTable::id_of("BTCUSDT"));
    EXPECT_EQ(kline.base.ts, 1717200000000LL * 1'000'000);     // ms -> ns
    EXPECT_EQ(kline.close_time, 1717200299999LL * 1'000'000);  // ms -> ns
    EXPECT_DOUBLE_EQ(kline.open, 67577.90);
    EXPECT_DOUBLE_EQ(kline.high, 67680.70);
    EXPECT_DOUBLE_EQ(kline.low, 67572.00);
    EXPECT_DOUBLE_EQ(kline.close, 67680.40);
    EXPECT_DOUBLE_EQ(kline.volume, 464.748);
}

TEST(BinHistVenue, RejectsAKlineRowForAnUnknownSymbol) {
    constexpr std::string_view kUnknown =
        "NOTASYMBOL,K,1717200000000,67577.90,67680.70,67572.00,67680.40,464.748,1717200299999,"
        "31428593.42840,6933,289.996,19611103.65640,0";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kUnknown)).has_value());
}

TEST(BinHistVenue, RejectsAKlineRowWithTooFewFields) {
    constexpr std::string_view kTruncated = "BTCUSDT,K,1717200000000,67577.90";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kTruncated)).has_value());
}

TEST(BinHistVenue, ParsesARealMarkPriceRow) {
    auto ev = BinHistVenue::parse(as_bytes(kRealMarkLine));
    ASSERT_TRUE(ev.has_value());
    ASSERT_TRUE(std::holds_alternative<MarkPriceKlineEvent>(*ev));
    const auto& mark = std::get<MarkPriceKlineEvent>(*ev);
    EXPECT_EQ(mark.base.symbol, *BinHistSymbolTable::id_of("BTCUSDT"));
    EXPECT_EQ(mark.base.ts, 1717200000000LL * 1'000'000);     // ms -> ns
    EXPECT_EQ(mark.close_time, 1717200299999LL * 1'000'000);  // ms -> ns
    EXPECT_DOUBLE_EQ(mark.open, 67570.93117730);
    EXPECT_DOUBLE_EQ(mark.high, 67680.70000000);
    EXPECT_DOUBLE_EQ(mark.low, 67570.93117730);
    EXPECT_DOUBLE_EQ(mark.close, 67678.20000000);
}

TEST(BinHistVenue, RejectsAMarkPriceRowWithTooFewFields) {
    constexpr std::string_view kTruncated = "BTCUSDT,M,1717200000000,67570.93117730";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kTruncated)).has_value());
}
