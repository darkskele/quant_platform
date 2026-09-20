#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "client.hpp"
#include "endpoints.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::BinanceMarket;
using binance::Cadence;
using binance::CadenceSupport;
using binance::Endpoint;
using binance::endpoint;
using binance::EndpointKind;
using binance::fallback_cadence;
using binance::file_name;
using binance::file_url;
using binance::kEndpoints;
using binance::market_of_slot;
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

TEST(BinanceEndpoints, CoinMPrefixes) {
    const auto& klines = endpoint(BinanceMarket::CoinM, EndpointKind::Klines);
    EXPECT_EQ(prefix(klines, "BTCUSD_PERP", "1h", Cadence::Daily),
              "data/futures/cm/daily/klines/BTCUSD_PERP/1h/");

    const auto& funding = endpoint(BinanceMarket::CoinM, EndpointKind::FundingRate);
    EXPECT_EQ(prefix(funding, "BTCUSD_PERP", "", Cadence::Monthly),
              "data/futures/cm/monthly/fundingRate/BTCUSD_PERP/");
}

// The enum doubles as the subscription's market slot numbering.
TEST(BinanceEndpoints, MarketSlotsMapToPaths) {
    ASSERT_NE(market_of_slot(0), nullptr);
    ASSERT_NE(market_of_slot(1), nullptr);
    ASSERT_NE(market_of_slot(2), nullptr);
    EXPECT_EQ(*market_of_slot(0), BinanceMarket::Spot);
    EXPECT_EQ(*market_of_slot(1), BinanceMarket::UsdM);
    EXPECT_EQ(*market_of_slot(2), BinanceMarket::CoinM);
    EXPECT_EQ(market_of_slot(3), nullptr);
}

TEST(BinanceEndpoints, SpotHasNoFundingRate) {
    EXPECT_THROW((void)endpoint(BinanceMarket::Spot, EndpointKind::FundingRate), std::out_of_range);
}

TEST(BinanceEndpoints, SpotHasNoMarkPriceKlines) {
    EXPECT_THROW((void)endpoint(BinanceMarket::Spot, EndpointKind::MarkPriceKlines),
                 std::out_of_range);
}

TEST(BinanceEndpoints, MetricsPathsAndCadence) {
    const auto& e = endpoint(BinanceMarket::UsdM, EndpointKind::Metrics);
    EXPECT_EQ(prefix(e, "BTCUSDT", "", Cadence::Daily), "data/futures/um/daily/metrics/BTCUSDT/");
    EXPECT_EQ(file_name(e, "BTCUSDT", "", "2025-06-02"), "BTCUSDT-metrics-2025-06-02.zip");
    EXPECT_TRUE(supports(e, Cadence::Daily));
    EXPECT_FALSE(supports(e, Cadence::Monthly));
    EXPECT_EQ(fallback_cadence(e), Cadence::Daily);

    const auto& cm = endpoint(BinanceMarket::CoinM, EndpointKind::Metrics);
    EXPECT_EQ(prefix(cm, "BTCUSD_PERP", "", Cadence::Daily),
              "data/futures/cm/daily/metrics/BTCUSD_PERP/");
}

TEST(BinanceEndpoints, SpotHasNoMetrics) {
    EXPECT_THROW((void)endpoint(BinanceMarket::Spot, EndpointKind::Metrics), std::out_of_range);
}

// DailyOnly exists for metrics and bookDepth, which 404 on monthly. Before it
// there was no way to say that, and supports() answered true for monthly on
// every dataset.
TEST(BinanceEndpoints, CadenceSupportIsTwoWay) {
    constexpr Endpoint daily{"futures/um", "metrics", CadenceSupport::DailyOnly, false, 8, 0};
    EXPECT_TRUE(supports(daily, Cadence::Daily));
    EXPECT_FALSE(supports(daily, Cadence::Monthly));

    constexpr Endpoint monthly{
        "futures/um", "fundingRate", CadenceSupport::MonthlyOnly, false, 3, 0};
    EXPECT_FALSE(supports(monthly, Cadence::Daily));
    EXPECT_TRUE(supports(monthly, Cadence::Monthly));

    constexpr Endpoint both{"futures/um", "klines", CadenceSupport::Both, true, 12, 5};
    EXPECT_TRUE(supports(both, Cadence::Daily));
    EXPECT_TRUE(supports(both, Cadence::Monthly));
}

TEST(BinanceEndpoints, FallbackCadencePrefersMonthlyUnlessDailyOnly) {
    constexpr Endpoint daily{"futures/um", "metrics", CadenceSupport::DailyOnly, false, 8, 0};
    EXPECT_EQ(fallback_cadence(daily), Cadence::Daily);

    constexpr Endpoint monthly{
        "futures/um", "fundingRate", CadenceSupport::MonthlyOnly, false, 3, 0};
    EXPECT_EQ(fallback_cadence(monthly), Cadence::Monthly);

    constexpr Endpoint both{"futures/um", "klines", CadenceSupport::Both, true, 12, 5};
    EXPECT_EQ(fallback_cadence(both), Cadence::Monthly);
}

// The invariant the source's add() leans on. A row whose fallback is not
// published would plan a prefix of files that do not exist.
TEST(BinanceEndpoints, EveryEntryFallsBackToACadenceItPublishes) {
    for (const auto& e : kEndpoints) {
        SCOPED_TRACE(std::string(e.market_path) + "/" + std::string(e.kind_path));
        EXPECT_TRUE(supports(e, fallback_cadence(e)));
        EXPECT_TRUE(supports(e, Cadence::Daily) || supports(e, Cadence::Monthly));
    }
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
        {BinanceMarket::UsdM, EndpointKind::Metrics, Cadence::Daily, "2025-06-02"},
    };

    const Case coin_m[] = {
        {BinanceMarket::CoinM, EndpointKind::Klines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::CoinM, EndpointKind::MarkPriceKlines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::CoinM, EndpointKind::PremiumIndexKlines, Cadence::Daily, "2025-06-02"},
        {BinanceMarket::CoinM, EndpointKind::FundingRate, Cadence::Monthly, "2025-06"},
        {BinanceMarket::CoinM, EndpointKind::Metrics, Cadence::Daily, "2025-06-02"},
    };

    for (const auto& c : coin_m) {
        const auto& e   = endpoint(c.market, c.kind);
        const auto  url = file_url(e, "BTCUSD_PERP", "1h", c.cadence, c.stamp);
        SCOPED_TRACE(url);
        EXPECT_FALSE(qp::data_source::network::http::get(url).empty());
    }

    for (const auto& c : cases) {
        const auto& e   = endpoint(c.market, c.kind);
        const auto  url = file_url(e, "BTCUSDT", "1h", c.cadence, c.stamp);
        SCOPED_TRACE(url);
        EXPECT_FALSE(qp::data_source::network::http::get(url).empty());
    }
}
