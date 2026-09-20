#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <tuple>
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

namespace binance = qp::data_source::source::venue::binance;

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
