#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <variant>
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

/// Datetime stamped rows, the shape metrics publishes. Minutes are enough to
/// separate samples inside one day.
std::string metrics_file(const std::vector<int>& minutes, std::string_view symbol = "BTCUSDT") {
    std::string out =
        "create_time,symbol,sum_open_interest,sum_open_interest_value,"
        "count_toptrader_long_short_ratio,sum_toptrader_long_short_ratio,count_long_short_ratio,"
        "sum_taker_long_short_vol_ratio\n";
    for (auto minute : minutes) {
        out += "2024-01-01 00:";
        out += minute < 10 ? "0" : "";
        out += std::to_string(minute);
        out += ":00,";
        out += symbol;
        out += ",100.5,2000000.25,1.1,1.2,1.3,1.4\n";
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

// 2024-01-01 00:00 and 00:05 UTC, the stamps metrics_file writes.
namespace {
constexpr Timestamp kJan1         = 1704067200LL * 1'000'000'000LL;
constexpr Timestamp kJan1Plus5Min = kJan1 + 300LL * 1'000'000'000LL;
}  // namespace

TEST(BinanceSource, MetricsMergesWithKlinesInTimestampOrder) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(config({{EndpointKind::Klines, "1h"}, {EndpointKind::Metrics, ""}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    deliver_all(pool, {kline_file({1704067200000}), metrics_file({0, 5})});

    std::vector<std::pair<Timestamp, EventKind>> seen;
    for (;;) {
        auto pulled = source.next();
        if (!pulled) {
            ASSERT_EQ(pulled.error(), SourceStatus::Eof);
            break;
        }
        seen.push_back({pulled->base.ts, pulled->base.kind});
    }

    ASSERT_EQ(seen.size(), 3u);
    EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end(),
                               [](const auto& a, const auto& b) { return a.first < b.first; }));
    EXPECT_EQ(seen.back().first, kJan1Plus5Min);
    EXPECT_EQ(seen.back().second, EventKind::OpenInterest);
}

TEST(BinanceSource, MetricsCarriesItsPayload) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(config({{EndpointKind::Metrics, ""}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    ASSERT_TRUE(pool.deliver(metrics_file({0})));

    auto pulled = source.next();
    ASSERT_TRUE(pulled.has_value());
    EXPECT_EQ(pulled->base.ts, kJan1);
    const auto* payload = std::get_if<qp::OpenInterestEvent>(&pulled->payload);
    ASSERT_NE(payload, nullptr);
    EXPECT_DOUBLE_EQ(payload->open_interest, 100.5);
    EXPECT_DOUBLE_EQ(payload->open_interest_value, 2000000.25);
}

// Spot publishes no metrics, so the stream is simply not built, the same way
// spot funding is skipped.
TEST(BinanceSource, SpotMetricsBuildsNoStream) {
    const auto subs = universe({"BTCUSDT"}, kSpot);
    Source     source(config({{EndpointKind::Klines, "1h"}, {EndpointKind::Metrics, ""}}), subs);

    EXPECT_EQ(source.stream_count(), 1u);
}

// metrics is daily only. Asking for monthly must plan daily rather than build a
// monthly prefix that 404s.
TEST(BinanceSource, MetricsFallsBackToDailyWhenMonthlyAsked) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(
        config({{EndpointKind::Klines, "1h"}, {EndpointKind::Metrics, ""}}, Cadence::Monthly),
        subs);

    std::vector<std::string> prefixes;
    source.plan_with([&prefixes](std::string_view prefix) {
        prefixes.emplace_back(prefix);
        return std::vector<std::string>{};
    });

    ASSERT_EQ(prefixes.size(), 2u);
    const auto metrics = std::find_if(prefixes.begin(), prefixes.end(), [](const std::string& p) {
        return p.find("metrics") != std::string::npos;
    });
    ASSERT_NE(metrics, prefixes.end());
    EXPECT_NE(metrics->find("/daily/"), std::string::npos) << *metrics;

    // The kline stream asked for monthly and gets it, so the fallback is per
    // endpoint and not a global downgrade.
    const auto klines = std::find_if(prefixes.begin(), prefixes.end(), [](const std::string& p) {
        return p.find("klines") != std::string::npos;
    });
    ASSERT_NE(klines, prefixes.end());
    EXPECT_NE(klines->find("/monthly/"), std::string::npos) << *klines;
}

namespace {

/// One full bookDepth sample at 2024-01-01 00:<minute>:00.
std::string depth_file(const std::vector<int>& minutes) {
    std::string out = "timestamp,percentage,depth,notional\n";
    for (auto minute : minutes)
        for (int band : {-5, -4, -3, -2, -1, 1, 2, 3, 4, 5}) {
            out += "2024-01-01 00:";
            out += minute < 10 ? "0" : "";
            out += std::to_string(minute);
            out += ":00,";
            out += std::to_string(band);
            out += ",10.0,1000.0\n";
        }
    return out;
}

}  // namespace

// A grouped stream sits in the merge like any other, one event per sample.
TEST(BinanceSource, BookDepthMergesWithMetrics) {
    const auto subs = universe({"BTCUSDT"});
    Source     source(config({{EndpointKind::Metrics, ""}, {EndpointKind::BookDepth, ""}}), subs);
    auto&      pool = source.pool();
    source.plan_with(single_file_lister());

    ASSERT_FALSE(source.next().has_value());
    deliver_all(pool, {metrics_file({0, 5}), depth_file({1, 3})});

    std::vector<std::pair<Timestamp, EventKind>> seen;
    for (;;) {
        auto pulled = source.next();
        if (!pulled) {
            ASSERT_EQ(pulled.error(), SourceStatus::Eof);
            break;
        }
        if (pulled->base.kind == EventKind::BookDepth) {
            const auto* depth = std::get_if<qp::BookDepthEvent>(&pulled->payload);
            ASSERT_NE(depth, nullptr);
            ASSERT_NE(depth->bands, nullptr);
        }
        seen.push_back({pulled->base.ts, pulled->base.kind});
    }

    constexpr Timestamp                                kMinute  = 60LL * 1'000'000'000LL;
    const std::vector<std::pair<Timestamp, EventKind>> expected = {
        {kJan1, EventKind::OpenInterest},
        {kJan1 + kMinute, EventKind::BookDepth},
        {kJan1 + 3 * kMinute, EventKind::BookDepth},
        {kJan1Plus5Min, EventKind::OpenInterest},
    };
    EXPECT_EQ(seen, expected);
}
