#include <gtest/gtest.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

#include "bin_hist_venue.hpp"

using qp::EventKind;
using qp::FundingEvent;
using qp::KlineEvent;
using qp::data_source::source::venue::binance::binance_historical::BinHistSymbolTable;
using qp::data_source::source::venue::binance::binance_historical::BinHistVenue;

namespace {

std::span<const std::byte> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

}  // namespace

// Real shape, verified against a live fapi.binance.com/fapi/v1/fundingRate
// response (2026-08-29) — see bin_hist_parser_policy.hpp's own comment.
constexpr std::string_view kRealFundingEntry =
    R"({"symbol":"BTCUSDT","fundingTime":1787990400000,"fundingRate":"0.00010000",)"
    R"("markPrice":"77597.93110145","rateType":"Regular"})";

TEST(BinHistVenue, ParsesARealFundingEntry) {
    auto ev = BinHistVenue::parse(as_bytes(kRealFundingEntry));
    ASSERT_TRUE(ev.has_value());
    ASSERT_TRUE(std::holds_alternative<FundingEvent>(*ev));
    const auto& funding = std::get<FundingEvent>(*ev);
    EXPECT_EQ(funding.symbol, *BinHistSymbolTable::id_of("BTCUSDT"));
    EXPECT_DOUBLE_EQ(funding.funding_rate, 0.0001);
    EXPECT_DOUBLE_EQ(funding.mark_price, 77597.93110145);
    EXPECT_EQ(funding.ts, 1787990400000LL * 1'000'000);  // ms -> ns
}

TEST(BinHistVenue, RejectsAFundingEntryForAnUnknownSymbol) {
    constexpr std::string_view kUnknown =
        R"({"symbol":"NOTASYMBOL","fundingTime":1787990400000,"fundingRate":"0.00010000",)"
        R"("markPrice":"77597.93110145","rateType":"Regular"})";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kUnknown)).has_value());
}

TEST(BinHistVenue, RejectsMalformedJson) {
    constexpr std::string_view kBroken = R"({"symbol":"BTCUSDT","fundingRate":)";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kBroken)).has_value());
}

TEST(BinHistVenue, RejectsAFundingEntryMissingARequiredField) {
    constexpr std::string_view kMissingRate =
        R"({"symbol":"BTCUSDT","fundingTime":1787990400000,"markPrice":"77597.93110145"})";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kMissingRate)).has_value());
}

TEST(BinHistVenue, RejectsEmptyInput) {
    EXPECT_FALSE(BinHistVenue::parse(as_bytes("")).has_value());
    EXPECT_FALSE(BinHistVenue::parse(as_bytes("   \n")).has_value());
}

// Real shape, verified against a live data.binance.vision daily kline CSV
// (2026-08-29 session) — BTCUSDT-1h-2024-06-01.csv's first row, with the
// symbol prepended per bin_hist_parser_policy.hpp's documented Source
// convention (the raw file has no symbol field at all).
constexpr std::string_view kRealKlineRow =
    "BTCUSDT,1717200000000,67577.90,67729.90,67535.40,67690.00,3025.451,1717203599999,"
    "204628990.86440,49741,1569.014,106127949.20210,0";

TEST(BinHistVenue, ParsesARealKlineRow) {
    auto ev = BinHistVenue::parse(as_bytes(kRealKlineRow));
    ASSERT_TRUE(ev.has_value());
    ASSERT_TRUE(std::holds_alternative<KlineEvent>(*ev));
    const auto& kline = std::get<KlineEvent>(*ev);
    EXPECT_EQ(kline.symbol, *BinHistSymbolTable::id_of("BTCUSDT"));
    EXPECT_EQ(kline.ts, 1717200000000LL * 1'000'000);          // ms -> ns
    EXPECT_EQ(kline.close_time, 1717203599999LL * 1'000'000);  // ms -> ns
    EXPECT_DOUBLE_EQ(kline.open, 67577.90);
    EXPECT_DOUBLE_EQ(kline.high, 67729.90);
    EXPECT_DOUBLE_EQ(kline.low, 67535.40);
    EXPECT_DOUBLE_EQ(kline.close, 67690.00);
    EXPECT_DOUBLE_EQ(kline.volume, 3025.451);
}

TEST(BinHistVenue, RejectsAKlineRowForAnUnknownSymbol) {
    constexpr std::string_view kUnknown =
        "NOTASYMBOL,1717200000000,67577.90,67729.90,67535.40,67690.00,3025.451,1717203599999,"
        "204628990.86440,49741,1569.014,106127949.20210,0";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kUnknown)).has_value());
}

TEST(BinHistVenue, RejectsAKlineRowWithTooFewFields) {
    constexpr std::string_view kTruncated = "BTCUSDT,1717200000000,67577.90";
    EXPECT_FALSE(BinHistVenue::parse(as_bytes(kTruncated)).has_value());
}
