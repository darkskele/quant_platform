#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "binance_historical_source.hpp"
#include "client.hpp"
#include "control_channel.hpp"
#include "exchange.hpp"
#include "http_fetch_pool.hpp"
#include "run_data_source.hpp"
#include "subscription.hpp"
#include "types.hpp"
#include "zip.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::BinanceHistoricalConfig;
using binance::BinanceHistoricalSource;
using binance::BinanceMarket;
using binance::Cadence;
using binance::EndpointKind;
using binance::HttpFetchPool;
using binance::HttpFetchPoolConfig;
using binance::StreamSpec;
using qp::ControlChannel;
using qp::EventKind;
using qp::MarketEvent;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::Timestamp;

namespace {

// 2025-01-01 and 2025-03-31 as nanoseconds. Three monthly files per stream, and
// spot is microseconds over this span while futures is milliseconds, so one run
// covers both stamp units.
constexpr Timestamp kFrom = 1735689600000000000LL;
constexpr Timestamp kTo   = 1743379200000000000LL;

constexpr std::string_view kMonths[] = {"2025-01", "2025-02", "2025-03"};

constexpr std::uint16_t kSpot  = static_cast<std::uint16_t>(BinanceMarket::Spot);
constexpr std::uint16_t kUsdM  = static_cast<std::uint16_t>(BinanceMarket::UsdM);
constexpr std::uint16_t kCoinM = static_cast<std::uint16_t>(BinanceMarket::CoinM);

/// One event flattened for comparison. Symbol travels as a name so the oracle
/// never depends on the source's slot assignment.
struct Row {
    Timestamp     ts{};
    std::uint16_t market{};
    std::string   symbol;
    EventKind     kind{};
    double        a{}, b{}, c{}, d{}, e{};

    auto key() const { return std::tie(ts, market, symbol, kind, a, b, c, d, e); }

    bool operator==(const Row& other) const { return key() == other.key(); }

    bool operator<(const Row& other) const { return key() < other.key(); }
};

// Two symbols per market, so hourly bars collide on the same timestamp across
// symbols and across markets. Ties are the normal case here, not an edge.
Subscription build_universe() {
    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kSpot, "BTCUSDT");
    builder.add(qp::ExchangeId::Binance, kSpot, "ETHUSDT");
    builder.add(qp::ExchangeId::Binance, kUsdM, "BTCUSDT");
    builder.add(qp::ExchangeId::Binance, kUsdM, "ETHUSDT");
    builder.add(qp::ExchangeId::Binance, kCoinM, "BTCUSD_PERP");
    builder.add(qp::ExchangeId::Binance, kCoinM, "ETHUSD_PERP");
    return std::move(builder).build();
}

HttpFetchPoolConfig pool_config() {
    HttpFetchPoolConfig config;
    config.workers = 8;
    return config;
}

BinanceHistoricalConfig build_config() {
    return BinanceHistoricalConfig{.streams = {StreamSpec{EndpointKind::Klines, "1h"},
                                               StreamSpec{EndpointKind::MarkPriceKlines, "1h"},
                                               StreamSpec{EndpointKind::FundingRate, ""}},
                                   .pool    = pool_config(),
                                   .cadence = Cadence::Monthly,
                                   .from    = kFrom,
                                   .to      = kTo};
}

/// Collects everything handed to it. Never refuses, so nothing is retried.
class CollectingSink {
   public:
    bool record(MarketEvent&& event) {
        events.push_back(std::move(event));
        return true;
    }

    std::vector<MarketEvent> events;
};

// ---------------------------------------------------------------------------
// Oracle. Builds its own urls, does its own csv parse, and never touches the
// endpoints table, the parsers or the stream.
// ---------------------------------------------------------------------------

struct Expected {
    std::uint16_t market{};
    std::string   market_path;
    std::string   symbol;
    std::string   kind_path;
    std::string   interval;
    EventKind     kind{};
};

std::vector<Expected> expected_streams() {
    std::vector<Expected> out;
    const auto            add = [&out](std::uint16_t market, std::string path, std::string symbol) {
        out.push_back({market, path, symbol, "klines", "1h", EventKind::Kline});
        if (path == "spot") return;
        out.push_back({market, path, symbol, "markPriceKlines", "1h", EventKind::MarkPriceKline});
        out.push_back({market, path, symbol, "fundingRate", "", EventKind::Funding});
    };

    add(kSpot, "spot", "BTCUSDT");
    add(kSpot, "spot", "ETHUSDT");
    add(kUsdM, "futures/um", "BTCUSDT");
    add(kUsdM, "futures/um", "ETHUSDT");
    add(kCoinM, "futures/cm", "BTCUSD_PERP");
    add(kCoinM, "futures/cm", "ETHUSD_PERP");
    return out;
}

std::string expected_url(const Expected& stream, std::string_view month) {
    std::string url = "https://data.binance.vision/data/";
    url += stream.market_path;
    url += "/monthly/";
    url += stream.kind_path;
    url += '/';
    url += stream.symbol;
    url += '/';
    if (!stream.interval.empty()) {
        url += stream.interval;
        url += '/';
    }
    url += stream.symbol;
    url += '-';
    url += stream.interval.empty() ? stream.kind_path : stream.interval;
    url += '-';
    url += month;
    url += ".zip";
    return url;
}

std::vector<std::string> split(std::string_view row) {
    std::vector<std::string> out;
    std::size_t              start = 0;
    for (;;) {
        const auto comma = row.find(',', start);
        if (comma == std::string_view::npos) {
            out.emplace_back(row.substr(start));
            return out;
        }
        out.emplace_back(row.substr(start, comma - start));
        start = comma + 1;
    }
}

/// Deliberately a second implementation of the stamp rule, so a bug in the fast
/// one does not cancel out.
Timestamp to_nanos(const std::string& field) {
    const auto raw = std::stoll(field);
    return field.size() >= 16 ? raw * 1'000LL : raw * 1'000'000LL;
}

std::vector<Row> fetch_expected() {
    std::vector<Row> rows;

    for (const auto& stream : expected_streams()) {
        for (const auto& month : kMonths) {
            const auto             url   = expected_url(stream, month);
            const auto             zip   = qp::data_source::network::http::get(url);
            const auto             bytes = qp::data_source::archive::zip::unzip_single_entry(zip);
            const std::string_view csv(reinterpret_cast<const char*>(bytes.data()), bytes.size());

            std::size_t start = 0;
            while (start < csv.size()) {
                auto end = csv.find('\n', start);
                if (end == std::string_view::npos) end = csv.size();
                auto line = csv.substr(start, end - start);
                start     = end + 1;
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
                if (line.empty()) continue;
                if (line.front() < '0' || line.front() > '9') continue;

                const auto fields = split(line);
                Row row{.market = stream.market, .symbol = stream.symbol, .kind = stream.kind};
                row.ts = to_nanos(fields[0]);

                if (stream.kind == EventKind::Funding) {
                    row.a = std::stod(fields[2]);
                    row.b = static_cast<double>(std::stoll(fields[1]));
                } else {
                    row.a = std::stod(fields[1]);
                    row.b = std::stod(fields[2]);
                    row.c = std::stod(fields[3]);
                    row.d = std::stod(fields[4]);
                    // Mark klines carry no volume, and the source drops it.
                    // Coin-M counts contracts in column 5 and carries the base
                    // asset in column 7, which is the one that travels.
                    if (stream.kind == EventKind::Kline)
                        row.e = std::stod(fields[stream.market_path == "futures/cm" ? 7 : 5]);
                }
                rows.push_back(std::move(row));
            }
        }
    }
    return rows;
}

std::string symbol_name(const Subscription& universe, const MarketEvent& event) {
    const auto* exchange = universe.find_exchange(event.base.exchange);
    if (exchange == nullptr) return {};
    const auto* market = universe.find_market(*exchange, event.base.market);
    if (market == nullptr || event.base.symbol >= market->symbols.size()) return {};
    return market->symbols[event.base.symbol];
}

Row to_row(const Subscription& universe, const MarketEvent& event) {
    Row row{.ts     = event.base.ts,
            .market = event.base.market,
            .symbol = symbol_name(universe, event),
            .kind   = event.base.kind};

    std::visit(
        [&row](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, qp::KlineEvent>) {
                row.a = payload.open;
                row.b = payload.high;
                row.c = payload.low;
                row.d = payload.close;
                row.e = payload.volume;
            } else if constexpr (std::is_same_v<T, qp::MarkPriceKlineEvent>) {
                row.a = payload.open;
                row.b = payload.high;
                row.c = payload.low;
                row.d = payload.close;
            } else if constexpr (std::is_same_v<T, qp::FundingEvent>) {
                row.a = payload.funding_rate;
                row.b = static_cast<double>(payload.interval_hours);
            }
        },
        event.payload);
    return row;
}

}  // namespace

// Everything the source emits, against the same csvs fetched and parsed by an
// independent path. Proves ordering, completeness and field level agreement in
// one go.
TEST(BinanceHistoricalIntegration, MatchesTheRawCsvsExactly) {
    const auto universe = build_universe();

    CollectingSink sink;
    {
        BinanceHistoricalSource<HttpFetchPool> source(build_config(), universe);

        // Two symbols across three markets. Spot publishes klines only, the
        // two futures markets publish all three.
        ASSERT_EQ(source.stream_count(), 14u);
        source.plan();

        for (const auto& report : source.reports())
            EXPECT_EQ(report.stats.files_planned, 3u)
                << report.symbol << " " << static_cast<int>(report.kind);

        ControlChannel<1> control;
        const auto        consumer = control.attach();

        auto sources = std::forward_as_tuple(source);
        auto sinks   = std::forward_as_tuple(sink);
        qp::data_source::run_data_source(sources, sinks, control, consumer,
                                         std::chrono::milliseconds{1});

        const auto stats = source.pool().stats();
        EXPECT_EQ(stats.completed_failed, 0u) << "fetches failed";
        EXPECT_EQ(stats.submitted, 42u);

        for (const auto& report : source.reports()) {
            EXPECT_EQ(report.stats.files_failed, 0u) << report.symbol;
            EXPECT_EQ(report.stats.files_read, 3u) << report.symbol;
            EXPECT_EQ(report.stats.backwards_stamps, 0u) << report.symbol;
            EXPECT_EQ(report.stats.rows_rejected, 0u) << report.symbol;
            EXPECT_EQ(report.stats.blank_rows, 0u) << report.symbol;
        }
    }

    // Three months of hourly bars across ten streams. A floor, so an oracle
    // that silently fetched nothing cannot agree with a source that did.
    ASSERT_GT(sink.events.size(), 10000u);

    // Ordering is the source's job, so check it before sorting anything.
    for (std::size_t i = 1; i < sink.events.size(); ++i)
        ASSERT_LE(sink.events[i - 1].base.ts, sink.events[i].base.ts) << "out of order at " << i;

    std::vector<Row> produced;
    produced.reserve(sink.events.size());
    for (const auto& event : sink.events) produced.push_back(to_row(universe, event));

    auto expected = fetch_expected();

    ASSERT_EQ(produced.size(), expected.size());

    std::sort(produced.begin(), produced.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(produced, expected);

    // Both sides scale the same raw integer, so agreeing with each other says
    // nothing about the epoch itself. 2025-01-01T00:00:00Z is 1735689600
    // seconds, and the span starts there, so the first hourly bar must land on
    // it exactly. This is what catches a wrong unit or a wrong epoch.
    EXPECT_EQ(produced.front().ts, 1735689600000000000LL);
}

// Every hourly bar in a month should be there. Counts the source computed from
// the interval against what it actually parsed, so a short file shows up.
TEST(BinanceHistoricalIntegration, NoRowsMissingFromAnyIntervalledStream) {
    const auto universe = build_universe();

    BinanceHistoricalSource<HttpFetchPool> source(build_config(), universe);
    source.plan();

    MarketEvent event;
    for (;;) {
        auto pulled = source.next();
        if (!pulled && pulled.error() == qp::data_source::source::SourceStatus::Eof) break;
    }

    for (const auto& report : source.reports()) {
        if (report.interval.empty()) continue;  // funding has no derivable count
        EXPECT_EQ(report.stats.rows_expected, static_cast<std::int64_t>(report.stats.rows_parsed))
            << report.symbol << " market " << report.market;
    }
}

// Bars share a timestamp across symbols and markets constantly, so the order
// ties come out in has to be the same every run or a backtest is not
// reproducible. Nothing about the network timing may leak into it.
TEST(BinanceHistoricalIntegration, TieOrderIsReproducible) {
    const auto universe = build_universe();

    const auto run_once = [&universe] {
        BinanceHistoricalSource<HttpFetchPool> source(build_config(), universe);
        source.plan();

        std::vector<Row> rows;
        for (;;) {
            auto pulled = source.next();
            if (!pulled) {
                if (pulled.error() == qp::data_source::source::SourceStatus::Eof) break;
                continue;
            }
            rows.push_back(to_row(universe, *pulled));
        }
        return rows;
    };

    const auto first  = run_once();
    const auto second = run_once();

    ASSERT_GT(first.size(), 10000u);
    ASSERT_EQ(first.size(), second.size());
    EXPECT_EQ(first, second);

    // And there really are ties, or this proves nothing.
    std::size_t tied = 0;
    for (std::size_t i = 1; i < first.size(); ++i)
        if (first[i].ts == first[i - 1].ts) ++tied;
    EXPECT_GT(tied, 1000u);
}

// The plan's smallest end to end case. One symbol, one market, two datasets,
// two monthly files each, counted against the calendar rather than against the
// source's own bookkeeping.
TEST(BinanceHistoricalIntegration, OneSymbolTwoMonthsCountsEveryRow) {
    // 2025-01-01, 2025-02-01 and 2025-03-01. `to` selects files by their start
    // stamp rather than bars, so asking up to the first of February plans the
    // February file as well and the run covers both months.
    constexpr Timestamp kJan = 1735689600000000000LL;
    constexpr Timestamp kFeb = 1738368000000000000LL;
    constexpr Timestamp kMar = 1740787200000000000LL;

    // 31 and 28 days of hourly bars, and three funding settlements a day over
    // the same 59 days.
    constexpr std::size_t kKlines  = (31 + 28) * 24;
    constexpr std::size_t kFunding = (31 + 28) * 3;

    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kUsdM, "BTCUSDT");
    const auto universe = std::move(builder).build();

    CollectingSink sink;
    {
        BinanceHistoricalSource<HttpFetchPool> source(
            BinanceHistoricalConfig{
                .streams = {StreamSpec{EndpointKind::Klines, "1h"},
                            StreamSpec{EndpointKind::FundingRate, ""}},
                .pool    = pool_config(),
                .cadence = Cadence::Monthly,
                .from    = kJan,
                .to      = kFeb,
            },
            universe);

        ASSERT_EQ(source.stream_count(), 2u);
        source.plan();
        for (const auto& report : source.reports())
            ASSERT_EQ(report.stats.files_planned, 2u) << report.symbol;

        ControlChannel<1> control;
        const auto        consumer = control.attach();

        auto sources = std::forward_as_tuple(source);
        auto sinks   = std::forward_as_tuple(sink);
        qp::data_source::run_data_source(sources, sinks, control, consumer,
                                         std::chrono::milliseconds{1});

        EXPECT_EQ(source.pool().stats().completed_failed, 0u);
        for (const auto& report : source.reports()) {
            EXPECT_EQ(report.stats.files_read, 2u) << report.symbol;
            EXPECT_EQ(report.stats.files_failed, 0u) << report.symbol;
            EXPECT_EQ(report.stats.rows_rejected, 0u) << report.symbol;
            EXPECT_EQ(report.stats.backwards_stamps, 0u) << report.symbol;
        }
    }

    EXPECT_EQ(sink.events.size(), kKlines + kFunding);

    std::size_t klines = 0, funding = 0;
    Timestamp   previous = 0;
    for (const auto& event : sink.events) {
        EXPECT_EQ(event.base.market, kUsdM);
        EXPECT_EQ(event.base.symbol, 0u) << "only one symbol was subscribed";
        EXPECT_GE(event.base.ts, previous) << "the merge went backwards";
        previous = event.base.ts;

        if (event.base.kind == EventKind::Kline)
            ++klines;
        else if (event.base.kind == EventKind::Funding)
            ++funding;
        else
            ADD_FAILURE() << "unsubscribed kind " << static_cast<int>(event.base.kind);
    }

    EXPECT_EQ(klines, kKlines);
    EXPECT_EQ(funding, kFunding);

    // Anchors the epoch. Both sides of the pipeline scale the same raw integer,
    // so a count alone would not catch a unit or offset error.
    constexpr Timestamp kHour = 3600LL * 1'000'000'000LL;
    ASSERT_FALSE(sink.events.empty());
    EXPECT_EQ(sink.events.front().base.ts, kJan);
    EXPECT_EQ(sink.events.back().base.ts, kMar - kHour)
        << "the last bar should be the final hour of february";
}

// ---------------------------------------------------------------------------
// metrics. Daily only, so it gets its own span and its own oracle rather than
// widening the monthly one above.
// ---------------------------------------------------------------------------

namespace {

// 2025-06-02 00:00:00 and 23:59:59 UTC. One day, so the run stays seconds.
constexpr Timestamp kMetricsFrom = 1748822400LL * 1'000'000'000LL;
constexpr Timestamp kMetricsTo   = 1748908799LL * 1'000'000'000LL;

/// USD-M populates all four ratios, Coin-M leaves three empty. One symbol of
/// each covers both shapes the dataset ships in.
struct MetricsStream {
    std::uint16_t market{};
    std::string   market_path;
    std::string   symbol;
    bool          ratios_published{};
};

std::vector<MetricsStream> metrics_streams() {
    return {{kUsdM, "futures/um", "BTCUSDT", true}, {kCoinM, "futures/cm", "BTCUSD_PERP", false}};
}

Subscription build_metrics_universe() {
    SubscriptionBuilder builder;
    for (const auto& stream : metrics_streams())
        builder.add(qp::ExchangeId::Binance, stream.market, stream.symbol);
    return std::move(builder).build();
}

/// One open interest sample, flattened. Ratios stay out of the key so the
/// comparison is the same on both markets.
struct MetricsRow {
    Timestamp   ts{};
    std::string symbol;
    double      open_interest{};
    double      open_interest_value{};
    double      account_ratio{};
    double      position_ratio{};
    double      long_short_ratio{};
    double      taker_ratio{};

    auto key() const { return std::tie(ts, symbol, open_interest, open_interest_value); }

    bool operator<(const MetricsRow& other) const { return key() < other.key(); }
};

/// A blank ratio is one the venue did not publish. NaN on both sides then.
double optional_decimal(const std::string& field) {
    return field.empty() ? std::numeric_limits<double>::quiet_NaN() : std::stod(field);
}

/// Equal to within a few ulps, or both unpublished.
bool same_ratio(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
    return std::abs(a - b) <= 4 * std::numeric_limits<double>::epsilon() * std::abs(b);
}

/// A second implementation of the datetime rule, so a bug in the fast one does
/// not cancel out.
Timestamp datetime_to_nanos(const std::string& field) {
    std::tm    tm{};
    const auto parsed = std::sscanf(field.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &tm.tm_year,
                                    &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    EXPECT_EQ(parsed, 6) << field;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return static_cast<Timestamp>(timegm(&tm)) * 1'000'000'000LL;
}

std::vector<MetricsRow> fetch_expected_metrics() {
    std::vector<MetricsRow> rows;
    for (const auto& stream : metrics_streams()) {
        std::string url = "https://data.binance.vision/data/";
        url += stream.market_path;
        url += "/daily/metrics/";
        url += stream.symbol;
        url += '/';
        url += stream.symbol;
        url += "-metrics-2025-06-02.zip";

        const auto             zip   = qp::data_source::network::http::get(url);
        const auto             bytes = qp::data_source::archive::zip::unzip_single_entry(zip);
        const std::string_view csv(reinterpret_cast<const char*>(bytes.data()), bytes.size());

        std::size_t start = 0;
        while (start < csv.size()) {
            auto end = csv.find('\n', start);
            if (end == std::string_view::npos) end = csv.size();
            auto line = csv.substr(start, end - start);
            start     = end + 1;
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.empty()) continue;
            if (line.front() < '0' || line.front() > '9') continue;

            const auto fields = split(line);
            EXPECT_EQ(fields.size(), 8u) << line;
            if (fields.size() != 8) continue;
            rows.push_back(MetricsRow{.ts                  = datetime_to_nanos(fields[0]),
                                      .symbol              = stream.symbol,
                                      .open_interest       = std::stod(fields[2]),
                                      .open_interest_value = std::stod(fields[3]),
                                      .account_ratio       = optional_decimal(fields[4]),
                                      .position_ratio      = optional_decimal(fields[5]),
                                      .long_short_ratio    = optional_decimal(fields[6]),
                                      .taker_ratio         = std::stod(fields[7])});
        }
    }
    return rows;
}

}  // namespace

TEST(BinanceHistoricalIntegration, MetricsMatchesTheRawCsvs) {
    const auto universe = build_metrics_universe();

    CollectingSink sink;
    std::size_t    coin_m_seen = 0;
    {
        BinanceHistoricalConfig config{.streams = {StreamSpec{EndpointKind::Metrics, ""}},
                                       .pool    = pool_config(),
                                       // Monthly 404s for metrics, so this also
                                       // proves the daily fallback on a live path.
                                       .cadence = Cadence::Monthly,
                                       .from    = kMetricsFrom,
                                       .to      = kMetricsTo};

        BinanceHistoricalSource<HttpFetchPool> source(config, universe);
        ASSERT_EQ(source.stream_count(), 2u);
        source.plan();

        for (const auto& report : source.reports())
            EXPECT_EQ(report.stats.files_planned, 1u) << report.symbol;

        ControlChannel<1> control;
        const auto        consumer = control.attach();

        auto sources = std::forward_as_tuple(source);
        auto sinks   = std::forward_as_tuple(sink);
        qp::data_source::run_data_source(sources, sinks, control, consumer,
                                         std::chrono::milliseconds{1});

        EXPECT_EQ(source.pool().stats().completed_failed, 0u);
        for (const auto& report : source.reports()) {
            EXPECT_EQ(report.stats.files_read, 1u) << report.symbol;
            EXPECT_EQ(report.stats.rows_rejected, 0u)
                << report.symbol << " rejected rows, blank ratios must not reject";
            EXPECT_EQ(report.stats.backwards_stamps, 0u) << report.symbol;
            EXPECT_EQ(report.stats.repeated_stamps, 0u) << report.symbol;
        }
    }

    std::vector<MetricsRow> actual;
    actual.reserve(sink.events.size());
    for (const auto& event : sink.events) {
        ASSERT_EQ(event.base.kind, EventKind::OpenInterest);
        const auto* payload = std::get_if<qp::OpenInterestEvent>(&event.payload);
        ASSERT_NE(payload, nullptr);

        const auto symbol = symbol_name(universe, event);
        if (symbol == "BTCUSD_PERP") {
            ++coin_m_seen;
            EXPECT_TRUE(std::isnan(payload->toptrader_account_ratio)) << "coin-m leaves 4 empty";
            EXPECT_TRUE(std::isnan(payload->toptrader_position_ratio)) << "coin-m leaves 5 empty";
            EXPECT_TRUE(std::isnan(payload->account_long_short_ratio)) << "coin-m leaves 6 empty";
            EXPECT_FALSE(std::isnan(payload->taker_long_short_volume_ratio))
                << "coin-m does publish 7";
        } else {
            EXPECT_FALSE(std::isnan(payload->toptrader_account_ratio)) << "usd-m publishes 4";
        }

        actual.push_back(MetricsRow{.ts                  = event.base.ts,
                                    .symbol              = symbol,
                                    .open_interest       = payload->open_interest,
                                    .open_interest_value = payload->open_interest_value,
                                    .account_ratio       = payload->toptrader_account_ratio,
                                    .position_ratio      = payload->toptrader_position_ratio,
                                    .long_short_ratio    = payload->account_long_short_ratio,
                                    .taker_ratio         = payload->taker_long_short_volume_ratio});
    }

    EXPECT_GT(coin_m_seen, 0u) << "the blank ratio path was never exercised";

    auto expected = fetch_expected_metrics();
    ASSERT_FALSE(expected.empty());
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());

    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual[i].ts, expected[i].ts) << "row " << i;
        EXPECT_EQ(actual[i].symbol, expected[i].symbol) << "row " << i;
        EXPECT_DOUBLE_EQ(actual[i].open_interest, expected[i].open_interest) << "row " << i;
        EXPECT_DOUBLE_EQ(actual[i].open_interest_value, expected[i].open_interest_value)
            << "row " << i;
        EXPECT_DOUBLE_EQ(actual[i].taker_ratio, expected[i].taker_ratio) << "row " << i;
        EXPECT_TRUE(same_ratio(actual[i].account_ratio, expected[i].account_ratio)) << "row " << i;
        EXPECT_TRUE(same_ratio(actual[i].position_ratio, expected[i].position_ratio))
            << "row " << i;
        EXPECT_TRUE(same_ratio(actual[i].long_short_ratio, expected[i].long_short_ratio))
            << "row " << i;
    }

    // The merge is still ascending with two markets interleaved.
    Timestamp previous = 0;
    for (const auto& event : sink.events) {
        EXPECT_GE(event.base.ts, previous);
        previous = event.base.ts;
    }
}

// ---------------------------------------------------------------------------
// bookDepth. Ten rows per sample, so this proves the grouping end to end
// against an oracle that groups by its own rule.
// ---------------------------------------------------------------------------

namespace {

/// One sample flattened, bids then asks, each band as depth and notional.
struct DepthRow {
    Timestamp           ts{};
    std::string         symbol;
    std::vector<double> values;

    auto key() const { return std::tie(ts, symbol); }

    bool operator<(const DepthRow& other) const { return key() < other.key(); }
};

std::vector<DepthRow> fetch_expected_depth() {
    std::vector<DepthRow> out;
    for (const auto& stream : metrics_streams()) {
        std::string url = "https://data.binance.vision/data/";
        url += stream.market_path;
        url += "/daily/bookDepth/";
        url += stream.symbol;
        url += '/';
        url += stream.symbol;
        url += "-bookDepth-2025-06-02.zip";

        const auto             zip   = qp::data_source::network::http::get(url);
        const auto             bytes = qp::data_source::archive::zip::unzip_single_entry(zip);
        const std::string_view csv(reinterpret_cast<const char*>(bytes.data()), bytes.size());

        // Groups on the parsed stamp rather than the raw text, and places bands
        // by value rather than by position, so neither shortcut in the source
        // is repeated here.
        std::map<Timestamp, std::map<int, std::pair<double, double>>> samples;
        std::size_t                                                   start = 0;
        while (start < csv.size()) {
            auto end = csv.find('\n', start);
            if (end == std::string_view::npos) end = csv.size();
            auto line = csv.substr(start, end - start);
            start     = end + 1;
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.empty()) continue;
            if (line.front() < '0' || line.front() > '9') continue;

            const auto fields = split(line);
            EXPECT_EQ(fields.size(), 4u) << line;
            if (fields.size() != 4) continue;
            samples[datetime_to_nanos(fields[0])][std::stoi(fields[1])] = {std::stod(fields[2]),
                                                                           std::stod(fields[3])};
        }

        for (const auto& [ts, bands] : samples) {
            EXPECT_EQ(bands.size(), 10u) << stream.symbol << " at " << ts;
            DepthRow row{.ts = ts, .symbol = stream.symbol, .values = {}};
            for (int band : {-1, -2, -3, -4, -5, 1, 2, 3, 4, 5}) {
                const auto found = bands.find(band);
                row.values.push_back(found == bands.end() ? -1.0 : found->second.first);
                row.values.push_back(found == bands.end() ? -1.0 : found->second.second);
            }
            out.push_back(std::move(row));
        }
    }
    return out;
}

}  // namespace

TEST(BinanceHistoricalIntegration, BookDepthMatchesTheRawCsvs) {
    const auto universe = build_metrics_universe();

    CollectingSink sink;
    {
        BinanceHistoricalConfig config{.streams = {StreamSpec{EndpointKind::BookDepth, ""}},
                                       .pool    = pool_config(),
                                       .cadence = Cadence::Daily,
                                       .from    = kMetricsFrom,
                                       .to      = kMetricsTo};

        BinanceHistoricalSource<HttpFetchPool> source(config, universe);
        ASSERT_EQ(source.stream_count(), 2u);
        source.plan();

        ControlChannel<1> control;
        const auto        consumer = control.attach();

        auto sources = std::forward_as_tuple(source);
        auto sinks   = std::forward_as_tuple(sink);
        qp::data_source::run_data_source(sources, sinks, control, consumer,
                                         std::chrono::milliseconds{1});

        EXPECT_EQ(source.pool().stats().completed_failed, 0u);
        for (const auto& report : source.reports()) {
            EXPECT_EQ(report.stats.files_read, 1u) << report.symbol;
            EXPECT_EQ(report.stats.rows_rejected, 0u) << report.symbol;
            EXPECT_EQ(report.stats.backwards_stamps, 0u) << report.symbol;
            EXPECT_EQ(report.stats.repeated_stamps, 0u) << report.symbol;
            EXPECT_EQ(report.stats.rows_parsed % 10, 0u) << report.symbol;
        }
    }

    std::vector<DepthRow> actual;
    actual.reserve(sink.events.size());
    for (const auto& event : sink.events) {
        ASSERT_EQ(event.base.kind, EventKind::BookDepth);
        const auto* payload = std::get_if<qp::BookDepthEvent>(&event.payload);
        ASSERT_NE(payload, nullptr);
        ASSERT_NE(payload->bands, nullptr);

        DepthRow row{.ts = event.base.ts, .symbol = symbol_name(universe, event), .values = {}};
        for (const auto& band : payload->bands->bids) {
            row.values.push_back(band.depth);
            row.values.push_back(band.notional);
        }
        for (const auto& band : payload->bands->asks) {
            row.values.push_back(band.depth);
            row.values.push_back(band.notional);
        }
        actual.push_back(std::move(row));
    }

    auto expected = fetch_expected_depth();
    ASSERT_FALSE(expected.empty());
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());

    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        EXPECT_EQ(actual[i].ts, expected[i].ts) << "sample " << i;
        EXPECT_EQ(actual[i].symbol, expected[i].symbol) << "sample " << i;
        ASSERT_EQ(actual[i].values.size(), expected[i].values.size());
        for (std::size_t v = 0; v < actual[i].values.size(); ++v)
            EXPECT_DOUBLE_EQ(actual[i].values[v], expected[i].values[v])
                << "sample " << i << " value " << v;
    }

    Timestamp previous = 0;
    for (const auto& event : sink.events) {
        EXPECT_GE(event.base.ts, previous);
        previous = event.base.ts;
    }
}

// ---------------------------------------------------------------------------
// The new datasets beyond one day: ids across file boundaries, the duplicated
// early metrics, the alt the depth question is about, and all three merged.
// ---------------------------------------------------------------------------

namespace {

constexpr Timestamp kSecond = 1'000'000'000LL;

/// Runs a source to exhaustion and hands back what it emitted and reported.
struct Drained {
    std::vector<MarketEvent>           events;
    std::vector<binance::StreamReport> reports;
    std::size_t                        failed_fetches{};
};

Drained drain(std::vector<std::pair<std::uint16_t, std::string>> pairs,
              std::vector<EndpointKind> kinds, Timestamp from, Timestamp to) {
    SubscriptionBuilder builder;
    for (const auto& [market, symbol] : pairs) builder.add(qp::ExchangeId::Binance, market, symbol);
    const auto universe = std::move(builder).build();

    BinanceHistoricalConfig config{
        .streams = {}, .pool = pool_config(), .cadence = Cadence::Daily, .from = from, .to = to};
    for (const auto kind : kinds) config.streams.push_back(StreamSpec{kind, ""});

    Drained        out;
    CollectingSink sink;
    {
        BinanceHistoricalSource<HttpFetchPool> source(config, universe);
        source.plan();

        ControlChannel<1> control;
        const auto        consumer = control.attach();
        auto              sources  = std::forward_as_tuple(source);
        auto              sinks    = std::forward_as_tuple(sink);
        qp::data_source::run_data_source(sources, sinks, control, consumer,
                                         std::chrono::milliseconds{1});

        out.reports        = source.reports();
        out.failed_fetches = source.pool().stats().completed_failed;
    }
    out.events = std::move(sink.events);
    return out;
}

void expect_clean(const Drained& run) {
    EXPECT_EQ(run.failed_fetches, 0u);
    for (const auto& report : run.reports) {
        EXPECT_EQ(report.stats.files_failed, 0u) << report.symbol;
        EXPECT_EQ(report.stats.rows_rejected, 0u) << report.symbol;
        EXPECT_EQ(report.stats.backwards_stamps, 0u) << report.symbol;
        EXPECT_EQ(report.stats.sequence_gaps, 0u) << report.symbol;
    }
}

}  // namespace

// Three days of Coin-M trades. Ids carry on across both day boundaries, so the
// stream counts no gap and the ids are one unbroken run.
TEST(BinanceHistoricalIntegration, AggTradeIdsRunOnAcrossDays) {
    const auto run = drain({{kCoinM, "BTCUSD_PERP"}}, {EndpointKind::AggTrades}, kMetricsFrom,
                           kMetricsFrom + 3 * 86'400 * kSecond - kSecond);
    expect_clean(run);
    ASSERT_EQ(run.reports.size(), 1u);
    EXPECT_EQ(run.reports[0].stats.files_read, 3u);
    ASSERT_FALSE(run.events.empty());

    auto expected = std::get<qp::TradeEvent>(run.events.front().payload).id;
    for (const auto& event : run.events) {
        const auto& trade = std::get<qp::TradeEvent>(event.payload);
        ASSERT_EQ(trade.id, expected) << "the run of ids broke";
        ++expected;
    }
}

// The first months of metrics publish every row twice. They all arrive, and the
// stream counts exactly the repeats.
TEST(BinanceHistoricalIntegration, EarlyMetricsDuplicatesAreCountedExactly) {
    constexpr Timestamp kSep15 = 1600128000LL * kSecond;
    const auto          run    = drain({{kUsdM, "BTCUSDT"}}, {EndpointKind::Metrics}, kSep15,
                                       kSep15 + 86'400 * kSecond - kSecond);
    expect_clean(run);
    ASSERT_EQ(run.reports.size(), 1u);
    EXPECT_EQ(run.reports[0].stats.rows_parsed, 576u);
    EXPECT_EQ(run.reports[0].stats.repeated_stamps, 288u);
    EXPECT_EQ(run.events.size(), 576u);
}

// ONEUSDT is the alt whose tight top of book the depth question is about. Every
// sample of a real day has to parse whole.
TEST(BinanceHistoricalIntegration, BookDepthParsesAWholeDayForTheAlt) {
    const auto run =
        drain({{kUsdM, "ONEUSDT"}}, {EndpointKind::BookDepth}, kMetricsFrom, kMetricsTo);
    expect_clean(run);
    ASSERT_EQ(run.reports.size(), 1u);
    EXPECT_GT(run.events.size(), 2'000u) << "a day is roughly one sample every 30 seconds";
    EXPECT_EQ(run.reports[0].stats.rows_parsed, run.events.size() * 10);

    for (const auto& event : run.events) {
        const auto& bands = *std::get<qp::BookDepthEvent>(event.payload).bands;
        for (std::size_t k = 1; k < bands.bids.size(); ++k) {
            ASSERT_GE(bands.bids[k].depth, bands.bids[k - 1].depth) << "depth is cumulative";
            ASSERT_GE(bands.asks[k].depth, bands.asks[k - 1].depth) << "depth is cumulative";
        }
    }
}

// All three datasets on one symbol in one source. They interleave in timestamp
// order and each kind arrives exactly as often as its stream parsed it.
TEST(BinanceHistoricalIntegration, NewDatasetsMergeIntoOneOrderedStream) {
    const auto run =
        drain({{kCoinM, "BTCUSD_PERP"}},
              {EndpointKind::Metrics, EndpointKind::BookDepth, EndpointKind::AggTrades},
              kMetricsFrom, kMetricsTo);
    expect_clean(run);

    std::size_t metrics = 0, depth = 0, trades = 0;
    Timestamp   previous = 0;
    for (const auto& event : run.events) {
        ASSERT_GE(event.base.ts, previous) << "the merge went backwards";
        previous = event.base.ts;
        if (event.base.kind == EventKind::OpenInterest) ++metrics;
        if (event.base.kind == EventKind::BookDepth) ++depth;
        if (event.base.kind == EventKind::Trade) ++trades;
    }

    for (const auto& report : run.reports) {
        if (report.kind == EndpointKind::Metrics) {
            EXPECT_EQ(report.stats.rows_parsed, metrics);
        }
        if (report.kind == EndpointKind::BookDepth) {
            EXPECT_EQ(report.stats.rows_parsed, depth * 10);
        }
        if (report.kind == EndpointKind::AggTrades) {
            EXPECT_EQ(report.stats.rows_parsed, trades);
        }
    }
    EXPECT_GT(metrics, 0u);
    EXPECT_GT(depth, 0u);
    EXPECT_GT(trades, 0u);
}

// ---------------------------------------------------------------------------
// aggTrades against the raw csvs, every field, on every shape the dataset
// ships in: spot headerless with eight columns and microseconds, futures with
// a header, futures before the header existed, and a monthly file.
// ---------------------------------------------------------------------------

namespace {

struct TradesFile {
    std::uint16_t market{};
    std::string   market_path;
    std::string   symbol;
};

/// One trade flattened. Keyed on the aggregate id, which is unique per symbol.
struct TradeRow {
    std::string   symbol;
    std::uint64_t id{};
    Timestamp     ts{};
    std::uint64_t first_id{};
    std::uint64_t last_id{};
    double        price{};
    double        qty{};
    bool          sell{};

    auto key() const { return std::tie(symbol, id, ts, first_id, last_id, price, qty, sell); }

    bool operator<(const TradeRow& other) const { return key() < other.key(); }

    bool operator==(const TradeRow& other) const { return key() == other.key(); }
};

/// cadence is "daily" or "monthly", stamp the file's date in that cadence.
std::vector<TradeRow> fetch_expected_trades(const std::vector<TradesFile>& files,
                                            std::string_view cadence, std::string_view stamp) {
    std::vector<TradeRow> rows;
    for (const auto& file : files) {
        std::string url = "https://data.binance.vision/data/";
        url += file.market_path;
        url += '/';
        url += cadence;
        url += "/aggTrades/";
        url += file.symbol;
        url += '/';
        url += file.symbol;
        url += "-aggTrades-";
        url += stamp;
        url += ".zip";

        const auto             zip   = qp::data_source::network::http::get(url);
        const auto             bytes = qp::data_source::archive::zip::unzip_single_entry(zip);
        const std::string_view csv(reinterpret_cast<const char*>(bytes.data()), bytes.size());

        std::size_t start = 0;
        while (start < csv.size()) {
            auto end = csv.find('\n', start);
            if (end == std::string_view::npos) end = csv.size();
            auto line = csv.substr(start, end - start);
            start     = end + 1;
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.empty()) continue;
            if (line.front() < '0' || line.front() > '9') continue;

            const auto fields = split(line);
            EXPECT_GE(fields.size(), 7u) << line;
            if (fields.size() < 7) continue;
            rows.push_back(TradeRow{.symbol   = file.symbol,
                                    .id       = std::stoull(fields[0]),
                                    .ts       = to_nanos(fields[5]),
                                    .first_id = std::stoull(fields[3]),
                                    .last_id  = std::stoull(fields[4]),
                                    .price    = std::stod(fields[1]),
                                    .qty      = std::stod(fields[2]),
                                    .sell     = fields[6] == "true" || fields[6] == "True"});
        }
    }
    return rows;
}

/// Pulls the files through the source and checks every trade against the csv.
void expect_trades_match(const std::vector<TradesFile>& files, Cadence cadence, Timestamp from,
                         Timestamp to, std::string_view stamp) {
    std::vector<std::pair<std::uint16_t, std::string>> pairs;
    for (const auto& file : files) pairs.emplace_back(file.market, file.symbol);

    SubscriptionBuilder builder;
    for (const auto& [market, symbol] : pairs) builder.add(qp::ExchangeId::Binance, market, symbol);
    const auto universe = std::move(builder).build();

    BinanceHistoricalConfig config{.streams = {StreamSpec{EndpointKind::AggTrades, ""}},
                                   .pool    = pool_config(),
                                   .cadence = cadence,
                                   .from    = from,
                                   .to      = to};

    CollectingSink sink;
    {
        BinanceHistoricalSource<HttpFetchPool> source(config, universe);
        ASSERT_EQ(source.stream_count(), files.size());
        source.plan();

        // One file per stream. Anything more is a span that grew by accident.
        for (const auto& report : source.reports())
            ASSERT_EQ(report.stats.files_planned, 1u) << report.symbol;

        ControlChannel<1> control;
        const auto        consumer = control.attach();
        auto              sources  = std::forward_as_tuple(source);
        auto              sinks    = std::forward_as_tuple(sink);
        qp::data_source::run_data_source(sources, sinks, control, consumer,
                                         std::chrono::milliseconds{1});

        EXPECT_EQ(source.pool().stats().completed_failed, 0u);
        for (const auto& report : source.reports()) {
            EXPECT_EQ(report.stats.files_read, 1u) << report.symbol;
            EXPECT_EQ(report.stats.rows_rejected, 0u) << report.symbol;
            EXPECT_EQ(report.stats.backwards_stamps, 0u) << report.symbol;
            EXPECT_EQ(report.stats.sequence_gaps, 0u) << report.symbol << " is missing tape";
        }
    }

    std::vector<TradeRow> actual;
    actual.reserve(sink.events.size());
    Timestamp previous = 0;
    for (const auto& event : sink.events) {
        ASSERT_EQ(event.base.kind, EventKind::Trade);
        EXPECT_GE(event.base.ts, previous) << "the merge went backwards";
        previous = event.base.ts;

        const auto& trade = std::get<qp::TradeEvent>(event.payload);
        actual.push_back(TradeRow{.symbol   = symbol_name(universe, event),
                                  .id       = trade.id,
                                  .ts       = event.base.ts,
                                  .first_id = trade.first_trade_id,
                                  .last_id  = trade.last_trade_id,
                                  .price    = trade.price,
                                  .qty      = trade.qty,
                                  .sell     = trade.side == qp::Side::Sell});
    }

    auto expected =
        fetch_expected_trades(files, cadence == Cadence::Daily ? "daily" : "monthly", stamp);
    ASSERT_FALSE(expected.empty());
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());

    ASSERT_EQ(actual.size(), expected.size());
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < actual.size() && mismatches < 10; ++i) {
        if (actual[i] == expected[i]) continue;
        ++mismatches;
        ADD_FAILURE() << actual[i].symbol << " trade " << actual[i].id << " differs from the csv's "
                      << expected[i].id;
    }
    EXPECT_EQ(mismatches, 0u);
}

}  // namespace

// Today's shape on all three markets: spot headerless with eight columns and
// microsecond stamps, the futures with a header, Coin-M counting contracts.
TEST(BinanceHistoricalIntegration, AggTradesMatchTheRawCsvsOnEveryMarket) {
    expect_trades_match({{kSpot, "spot", "BTCUSDT"},
                         {kUsdM, "futures/um", "BTCUSDT"},
                         {kCoinM, "futures/cm", "BTCUSD_PERP"}},
                        Cadence::Daily, kMetricsFrom, kMetricsTo, "2025-06-02");
}

// USD-M before the header was added. The same parser has to take both.
TEST(BinanceHistoricalIntegration, AggTradesMatchTheHeaderlessFuturesShape) {
    constexpr Timestamp kJun2021 = 1622592000LL * kSecond;
    expect_trades_match({{kUsdM, "futures/um", "ONEUSDT"}}, Cadence::Daily, kJun2021,
                        kJun2021 + 86'400 * kSecond - kSecond, "2021-06-02");
}

// A month in one file, which is a different path and file name from a day.
TEST(BinanceHistoricalIntegration, AggTradesMatchAMonthlyFile) {
    constexpr Timestamp kMay2024 = 1714521600LL * kSecond;
    expect_trades_match({{kUsdM, "futures/um", "ONEUSDT"}}, Cadence::Monthly, kMay2024,
                        kMay2024 + 86'400 * kSecond, "2024-05");
}
