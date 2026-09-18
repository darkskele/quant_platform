#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "client.hpp"
#include "endpoints.hpp"

namespace binance = qp::data_source::source::venue::binance;

using binance::BinanceMarket;
using binance::Cadence;
using binance::endpoint;
using binance::EndpointKind;
using binance::file_name;
using binance::file_url;
using binance::prefix;
using binance::supports;

TEST(BinanceEndpoints, UsdMKlinesDailyPrefix) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::Klines);
    EXPECT_EQ(prefix(e, "BTCUSDT", "1h", Cadence::Daily),
              "data/futures/um/daily/klines/BTCUSDT/1h/");
}

TEST(BinanceEndpoints, UsdMKlinesMonthlyPrefix) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::Klines);
    EXPECT_EQ(prefix(e, "BTCUSDT", "1h", Cadence::Monthly),
              "data/futures/um/monthly/klines/BTCUSDT/1h/");
}

TEST(BinanceEndpoints, SpotKlinesPrefix) {
    const auto& e = endpoint(BinanceMarket::Spot, EndpointKind::Klines);
    EXPECT_EQ(prefix(e, "BTCUSDT", "1h", Cadence::Daily), "data/spot/daily/klines/BTCUSDT/1h/");
}

TEST(BinanceEndpoints, MarkAndPremiumKlinesPrefix) {
    const auto& mark = endpoint(BinanceMarket::UsdM, EndpointKind::MarkPriceKlines);
    EXPECT_EQ(prefix(mark, "BTCUSDT", "1h", Cadence::Daily),
              "data/futures/um/daily/markPriceKlines/BTCUSDT/1h/");

    const auto& premium = endpoint(BinanceMarket::UsdM, EndpointKind::PremiumIndexKlines);
    EXPECT_EQ(prefix(premium, "BTCUSDT", "1h", Cadence::Daily),
              "data/futures/um/daily/premiumIndexKlines/BTCUSDT/1h/");
}

// Funding carries no interval segment and no daily cadence.
TEST(BinanceEndpoints, FundingRatePrefixHasNoInterval) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::FundingRate);
    EXPECT_EQ(prefix(e, "BTCUSDT", "1h", Cadence::Monthly),
              "data/futures/um/monthly/fundingRate/BTCUSDT/");
}

TEST(BinanceEndpoints, IntervalledFileNameUsesInterval) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::Klines);
    EXPECT_EQ(file_name(e, "BTCUSDT", "1h", "2025-06-02"), "BTCUSDT-1h-2025-06-02.zip");
}

TEST(BinanceEndpoints, NonIntervalledFileNameUsesKind) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::FundingRate);
    EXPECT_EQ(file_name(e, "BTCUSDT", "1h", "2025-06"), "BTCUSDT-fundingRate-2025-06.zip");
}

TEST(BinanceEndpoints, FileUrlIsFullyQualified) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::Klines);
    EXPECT_EQ(file_url(e, "BTCUSDT", "1h", Cadence::Daily, "2025-06-02"),
              "https://data.binance.vision/data/futures/um/daily/klines/BTCUSDT/1h/"
              "BTCUSDT-1h-2025-06-02.zip");
}

TEST(BinanceEndpoints, CadenceSupport) {
    const auto& klines = endpoint(BinanceMarket::UsdM, EndpointKind::Klines);
    EXPECT_TRUE(supports(klines, Cadence::Daily));
    EXPECT_TRUE(supports(klines, Cadence::Monthly));

    const auto& funding = endpoint(BinanceMarket::UsdM, EndpointKind::FundingRate);
    EXPECT_FALSE(supports(funding, Cadence::Daily));
    EXPECT_TRUE(supports(funding, Cadence::Monthly));
}

TEST(BinanceEndpoints, ColumnCounts) {
    EXPECT_EQ(endpoint(BinanceMarket::UsdM, EndpointKind::Klines).column_count, 12);
    EXPECT_EQ(endpoint(BinanceMarket::UsdM, EndpointKind::MarkPriceKlines).column_count, 12);
    EXPECT_EQ(endpoint(BinanceMarket::UsdM, EndpointKind::PremiumIndexKlines).column_count, 12);
    EXPECT_EQ(endpoint(BinanceMarket::UsdM, EndpointKind::FundingRate).column_count, 3);
}

TEST(BinanceEndpoints, SpotHasNoFundingRate) {
    EXPECT_THROW((void)endpoint(BinanceMarket::Spot, EndpointKind::FundingRate), std::out_of_range);
}

TEST(BinanceEndpoints, SpotHasNoMarkPriceKlines) {
    EXPECT_THROW((void)endpoint(BinanceMarket::Spot, EndpointKind::MarkPriceKlines),
                 std::out_of_range);
}

// Every table entry against the live bucket. A path this builds wrong is a path
// no offline test can catch.
TEST(BinanceEndpointsLive, EveryEntryResolvesToARealFile) {
    struct Case {
        BinanceMarket    market;
        EndpointKind     kind;
        Cadence          cadence;
        std::string_view stamp;
    };

    const Case cases[] = {
        {BinanceMarket::Spot, EndpointKind::Klines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::UsdM, EndpointKind::Klines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::UsdM, EndpointKind::Klines, Cadence::Monthly, "2025-06"},
        {BinanceMarket::UsdM, EndpointKind::MarkPriceKlines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::UsdM, EndpointKind::PremiumIndexKlines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::UsdM, EndpointKind::FundingRate, Cadence::Monthly, "2025-06"},
    };

    for (const auto& c : cases) {
        const auto& e   = endpoint(c.market, c.kind);
        const auto  url = file_url(e, "BTCUSDT", "1h", c.cadence, c.stamp);
        SCOPED_TRACE(url);
        EXPECT_FALSE(qp::data_source::network::http::get(url).empty());
    }
}
