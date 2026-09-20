#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "binance_historical_source.hpp"
#include "support/fake_fetch_pool.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::BinanceHistoricalConfig;
using binance::BinanceHistoricalSource;
using binance::BinanceMarket;
using binance::Cadence;
using binance::EndpointKind;
using binance::StreamSpec;
using qp::EventKind;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::Timestamp;
using qp::data_source::source::SourceStatus;
using qp::testing::FakeFetchPool;

namespace {

// 2024-01 and 2024-03 as nanoseconds.
constexpr Timestamp kFrom = 1704067200000000000LL;
constexpr Timestamp kTo   = 1709251200000000000LL;

using Source = BinanceHistoricalSource<FakeFetchPool>;

// Market slots follow BinanceMarket, so the path needs no restating.
constexpr std::uint16_t kUsdM = static_cast<std::uint16_t>(BinanceMarket::UsdM);
constexpr std::uint16_t kSpot = static_cast<std::uint16_t>(BinanceMarket::Spot);

/// The run's subscription. Symbols come from here, not from the config.
Subscription universe(std::vector<std::string> symbols, std::uint16_t market = kUsdM) {
    SubscriptionBuilder builder;
    for (const auto& symbol : symbols) builder.add(qp::ExchangeId::Binance, market, symbol);
    return std::move(builder).build();
}

BinanceHistoricalConfig config(std::vector<StreamSpec> streams,
                               Cadence                 cadence = Cadence::Monthly) {
    return BinanceHistoricalConfig{
        .streams = std::move(streams), .cadence = cadence, .from = kFrom, .to = kTo};
}

std::string kline_file(const std::vector<long long>& open_times_ms) {
    std::string out =
        "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,"
        "taker_buy_quote_volume,ignore\n";
    for (auto ms : open_times_ms) {
        out += std::to_string(ms);
        out += ",1.0,2.0,0.5,1.5,10.0,";
        out += std::to_string(ms + 1);
        out += ",0,0,0,0,0\n";
    }
    return out;
}

std::string funding_file(const std::vector<long long>& calc_times_ms) {
    std::string out = "calc_time,funding_interval_hours,last_funding_rate\n";
    for (auto ms : calc_times_ms) {
        out += std::to_string(ms);
        out += ",8,0.0001\n";
    }
    return out;
}

/// Every stream plans one file, named by its prefix.
auto single_file_lister() {
    return [](std::string_view prefix) {
        std::string key(prefix);
        key += "FILE-2024-01.zip";
        return std::vector<std::string>{key};
    };
}

/// Serves each outstanding request in turn from a list of bodies.
void deliver_all(FakeFetchPool& pool, const std::vector<std::string>& bodies) {
    for (const auto& body : bodies) ASSERT_TRUE(pool.deliver(body));
}

}  // namespace

TEST(BinanceSource, BuildsOneStreamPerSymbolAndSpec) {
    const auto subs = universe({"BTCUSDT", "ETHUSDT"});
    Source source(config({{EndpointKind::Klines, "1h"}, {EndpointKind::FundingRate, ""}}), subs);

    EXPECT_EQ(source.stream_count(), 4u);
}

TEST(BinanceSource, ResolvesSymbolsThroughItsOwnSubscription) {
    const auto subs = universe({"BTCUSDT", "ETHUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}}), subs);

    EXPECT_EQ(source.stream_count(), 2u);
    const auto reports = source.reports();
    ASSERT_EQ(reports.size(), 2u);
    EXPECT_EQ(reports[0].market, kUsdM);
    EXPECT_NE(reports[0].symbol, reports[1].symbol);
}

// Nothing can be ordered until every stream has something to compare.
TEST(BinanceSource, NoDataUntilEveryStreamHasAnEvent) {
    const auto subs = universe({"BTCUSDT", "ETHUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    auto first = source.next();
    ASSERT_FALSE(first.has_value());
    EXPECT_EQ(first.error(), SourceStatus::NoData);

    // One of the two arrives. Still not enough to order.
    ASSERT_TRUE(pool.deliver(kline_file({1704067200000})));
    auto second = source.next();
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), SourceStatus::NoData);
}

TEST(BinanceSource, MergesTwoSymbolsInTimestampOrder) {
    const auto subs = universe({"BTCUSDT", "ETHUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    deliver_all(pool, {kline_file({1704067200000, 1704074400000}),
                       kline_file({1704070800000, 1704078000000})});

    std::vector<Timestamp> seen;
    for (;;) {
        auto pulled = source.next();
        if (!pulled) {
            ASSERT_EQ(pulled.error(), SourceStatus::Eof);
            break;
        }
        seen.push_back(pulled->base.ts);
    }

    ASSERT_EQ(seen.size(), 4u);
    EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
    EXPECT_EQ(seen.front(), 1704067200000LL * 1'000'000);
    EXPECT_EQ(seen.back(), 1704078000000LL * 1'000'000);
}

TEST(BinanceSource, MergesAcrossKinds) {
    const auto subs = universe({"BTCUSDT"});
    Source source(config({{EndpointKind::Klines, "1h"}, {EndpointKind::FundingRate, ""}}), subs);
    auto&  pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    deliver_all(pool, {kline_file({1704067200000, 1704074400000}), funding_file({1704070800000})});

    std::vector<EventKind> kinds;
    std::vector<Timestamp> seen;
    for (;;) {
        auto pulled = source.next();
        if (!pulled) break;
        kinds.push_back(pulled->base.kind);
        seen.push_back(pulled->base.ts);
    }

    ASSERT_EQ(kinds.size(), 3u);
    EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
    EXPECT_EQ(kinds[0], EventKind::Kline);
    EXPECT_EQ(kinds[1], EventKind::Funding);
    EXPECT_EQ(kinds[2], EventKind::Kline);
}

// A stream that finishes early must not hold the merge back.
TEST(BinanceSource, FinishedStreamStopsBlockingTheMerge) {
    const auto subs = universe({"BTCUSDT", "ETHUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    deliver_all(pool, {kline_file({1704067200000}),
                       kline_file({1704070800000, 1704074400000, 1704078000000})});

    int count = 0;
    while (source.next().has_value()) ++count;
    EXPECT_EQ(count, 4);
}

TEST(BinanceSource, ReportsCarrySymbolAndStats) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    ASSERT_TRUE(pool.deliver(kline_file({1704067200000, 1704074400000})));
    while (source.next().has_value()) {
    }

    const auto reports = source.reports();
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports.front().symbol, "BTCUSDT");
    EXPECT_EQ(reports.front().interval, "1h");
    EXPECT_EQ(reports.front().stats.rows_parsed, 2u);
    EXPECT_EQ(reports.front().stats.header_rows, 1u);
}

TEST(BinanceSource, EmptyPlanIsImmediateEof) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}}), subs);
    source.plan_with([](std::string_view) { return std::vector<std::string>{}; });

    auto pulled = source.next();
    ASSERT_FALSE(pulled.has_value());
    EXPECT_EQ(pulled.error(), SourceStatus::Eof);
}

// A slot outside Binance's market numbering is not one of its markets.
TEST(BinanceSource, UnknownMarketSlotIsSkipped) {
    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kUsdM, "BTCUSDT");
    builder.add(qp::ExchangeId::Binance, 99, "ETHUSDT");
    const auto subs = std::move(builder).build();

    Source source(config({{EndpointKind::Klines, "1h"}}), subs);

    EXPECT_EQ(source.stream_count(), 1u);
    ASSERT_EQ(source.reports().size(), 1u);
    EXPECT_EQ(source.reports().front().market, kUsdM);
}

// Spot publishes no funding, so the same config yields fewer streams there.
TEST(BinanceSource, DatasetsAMarketDoesNotPublishAreSkipped) {
    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kSpot, "BTCUSDT");
    builder.add(qp::ExchangeId::Binance, kUsdM, "BTCUSDT");
    const auto subs = std::move(builder).build();

    Source source(config({{EndpointKind::Klines, "1h"}, {EndpointKind::FundingRate, ""}}), subs);

    // Spot gets klines only, USD-M gets both.
    EXPECT_EQ(source.stream_count(), 3u);
}

// Both markets in one source, so one pool serves the whole run.
TEST(BinanceSource, SpansSpotAndFuturesFromOneSubscription) {
    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kSpot, "BTCUSDT");
    builder.add(qp::ExchangeId::Binance, kUsdM, "BTCUSDT");
    const auto subs = std::move(builder).build();

    Source source(config({{EndpointKind::Klines, "1h"}}), subs);

    std::vector<std::string> prefixes;
    source.plan_with([&prefixes](std::string_view prefix) {
        prefixes.emplace_back(prefix);
        return std::vector<std::string>{};
    });

    ASSERT_EQ(prefixes.size(), 2u);
    EXPECT_NE(std::find(prefixes.begin(), prefixes.end(), "data/spot/monthly/klines/BTCUSDT/1h/"),
              prefixes.end());
    EXPECT_NE(
        std::find(prefixes.begin(), prefixes.end(), "data/futures/um/monthly/klines/BTCUSDT/1h/"),
        prefixes.end());
}

// Funding is monthly only, so a daily config must not produce a daily path.
TEST(BinanceSource, FundingStaysMonthlyUnderADailyConfig) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(config({{EndpointKind::FundingRate, ""}}, Cadence::Daily), subs);

    std::vector<std::string> seen;
    source.plan_with([&seen](std::string_view prefix) {
        seen.emplace_back(prefix);
        return std::vector<std::string>{};
    });

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen.front(), "data/futures/um/monthly/fundingRate/BTCUSDT/");
}
