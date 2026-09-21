#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "parsers/agg_trades.hpp"
#include "parsers/book_depth.hpp"
#include "parsers/funding.hpp"
#include "parsers/klines.hpp"
#include "stream.hpp"
#include "support/fake_fetch_pool.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::BinanceHistoricalStream;
using binance::BinanceMarket;
using binance::Cadence;
using binance::parsers::AggTradesParser;
using binance::parsers::BookDepthParser;
using binance::parsers::FundingParser;
using binance::parsers::KlineParser;
using qp::EventKind;
using qp::KlineEvent;
using qp::MarketEvent;
using qp::Subscription;
using qp::testing::FakeFetchPool;

namespace {

constexpr Subscription::Instrument kInstrument{.exchange = 0, .market = 3, .symbol = 7};

// 2024-01-01 and 2024-02-01 as nanoseconds.
constexpr qp::Timestamp kJan = 1704067200000000000LL;
constexpr qp::Timestamp kFeb = 1706745600000000000LL;

using KlineStream   = BinanceHistoricalStream<KlineParser, FakeFetchPool>;
using FundingStream = BinanceHistoricalStream<FundingParser, FakeFetchPool>;
using DepthStream   = BinanceHistoricalStream<BookDepthParser, FakeFetchPool>;
using TradesStream  = BinanceHistoricalStream<AggTradesParser, FakeFetchPool>;

std::vector<std::string> daily_keys() {
    return {
        "data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2023-12-31.zip",
        "data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2024-01-01.zip",
        "data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2024-01-15.zip",
        "data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2024-03-01.zip",
    };
}

/// `count` consecutive daily keys from 2024-01-01, so a plan can be longer
/// than the fetch window.
std::vector<std::string> long_daily_keys(int count) {
    std::vector<std::string> keys;
    for (int day = 1; day <= count; ++day) {
        const std::string stamp = (day < 10 ? "0" : "") + std::to_string(day);
        keys.push_back("data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-2024-01-" + stamp +
                       ".zip");
    }
    return keys;
}

/// A two row file whose first bar opens at `open_ms`, so files delivered in any
/// order still carry ascending stamps when read in plan order.
std::string two_rows_from(long long open_ms) {
    std::string out =
        "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,"
        "taker_buy_quote_volume,ignore\n";
    for (int bar = 0; bar < 2; ++bar) {
        const long long open = open_ms + bar * 3'600'000LL;
        out += std::to_string(open);
        out += ",42000.10,42100.00,41900.00,42050.00,10.5,";
        // A zero stamp is refused, so the close time has to be a real one.
        out += std::to_string(open + 3'599'999LL);
        out += ",0,0,0,0,0\n";
    }
    return out;
}

/// Every bar twice, the shape the 2020 and early 2021 metrics files ship in.
std::string duplicated_rows_from(long long open_ms) {
    std::string out =
        "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,"
        "taker_buy_quote_volume,ignore\n";
    for (int bar = 0; bar < 2; ++bar) {
        const long long open = open_ms + bar * 3'600'000LL;
        for (int copy = 0; copy < 2; ++copy) {
            out += std::to_string(open);
            out += ",42000.10,42100.00,41900.00,42050.00,10.5,";
            out += std::to_string(open + 3'599'999LL);
            out += ",0,0,0,0,0\n";
        }
    }
    return out;
}

std::unique_ptr<KlineStream> make_kline_stream(FakeFetchPool& pool) {
    return std::make_unique<KlineStream>(pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "1h",
                                         kInstrument);
}

const std::string kTwoRowFile =
    "open_time,open,high,low,close,volume,close_time,quote_volume,count,taker_buy_volume,"
    "taker_buy_quote_volume,ignore\n"
    "1704067200000,42000.10,42100.00,41900.00,42050.00,10.5,1704070799999,0,0,0,0,0\n"
    "1704070800000,42050.00,42200.00,42000.00,42150.00,11.5,1704074399999,0,0,0,0,0\n";

int drain(KlineStream& stream) {
    MarketEvent event;
    int         count = 0;
    while (stream.next(event)) ++count;
    return count;
}

}  // namespace

TEST(BinanceStream, PlanKeepsOnlyKeysInsideSpan) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);
    EXPECT_EQ(stream->planned_files(), 2u);
}

TEST(BinanceStream, PlanReadsMonthlyStamps) {
    FakeFetchPool pool;
    FundingStream stream(pool, BinanceMarket::UsdM, Cadence::Monthly, "BTCUSDT", "", kInstrument);
    stream.plan(
        {
            "data/futures/um/monthly/fundingRate/BTCUSDT/BTCUSDT-fundingRate-2023-12.zip",
            "data/futures/um/monthly/fundingRate/BTCUSDT/BTCUSDT-fundingRate-2024-01.zip",
        },
        kJan, kFeb);
    EXPECT_EQ(stream.planned_files(), 1u);
}

// Scheduling is driven by next(), so nothing is asked for until it is called.
TEST(BinanceStream, NextSchedulesAFetch) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);
    EXPECT_EQ(pool.in_flight(), 0u);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(pool.in_flight(), 2u) << "both planned files fit the window";
    EXPECT_EQ(pool.submitted().front(),
              "https://data.binance.vision/data/futures/um/daily/klines/BTCUSDT/1h/"
              "BTCUSDT-1h-2024-01-01.zip");
}

// The window is what bounds concurrency, and one slot is held from the request
// until the file is read.
TEST(BinanceStream, FetchWindowFillsAndIsBounded) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(long_daily_keys(20), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(pool.in_flight(), stream->prefetch_depth());

    EXPECT_FALSE(stream->next(event));
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(pool.in_flight(), stream->prefetch_depth()) << "the window did not hold";
    EXPECT_EQ(pool.submitted().size(), stream->prefetch_depth());
}

// The depth is what bounds the window, and the ring is only its ceiling.
TEST(BinanceStream, PrefetchDepthIsConfigurableAndClamped) {
    FakeFetchPool pool;
    KlineStream shallow(pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "1h", kInstrument, 3);
    shallow.plan(long_daily_keys(20), kJan, kFeb);
    EXPECT_EQ(shallow.prefetch_depth(), 3u);

    MarketEvent event;
    EXPECT_FALSE(shallow.next(event));
    EXPECT_EQ(pool.in_flight(), 3u);

    FakeFetchPool greedy_pool;
    KlineStream   greedy(greedy_pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "1h",
                         kInstrument, binance::kFileSlotCount * 4);
    EXPECT_EQ(greedy.prefetch_depth(), binance::kFileSlotCount) << "asking past the ring";

    FakeFetchPool zero_pool;
    KlineStream   zero(zero_pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "1h", kInstrument,
                       0);
    EXPECT_EQ(zero.prefetch_depth(), 1u) << "a stream that asks for nothing never finishes";
}

// A slot frees on the read, not on the delivery, so that is when the next
// request goes out.
TEST(BinanceStream, ReadingAFileReleasesTheNextFetch) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(long_daily_keys(20), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(two_rows_from(1704067200000LL)));
    EXPECT_EQ(pool.submitted().size(), stream->prefetch_depth()) << "delivery is not a read";

    ASSERT_TRUE(stream->next(event));
    EXPECT_EQ(pool.submitted().size(), stream->prefetch_depth() + 1);
}

// The whole point of addressing slots by position. Fetches finish backwards and
// the reader still sees plan order.
TEST(BinanceStream, OutOfOrderCompletionsAreReadInPlanOrder) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(long_daily_keys(3), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    ASSERT_EQ(pool.in_flight(), 3u);

    // Newest first, so position 2 lands, then 1, then 0.
    ASSERT_TRUE(pool.deliver_newest(two_rows_from(1704240000000LL)));
    ASSERT_TRUE(pool.deliver_newest(two_rows_from(1704153600000LL)));
    EXPECT_FALSE(stream->next(event)) << "position 0 is still missing";

    ASSERT_TRUE(pool.deliver(two_rows_from(1704067200000LL)));

    std::vector<qp::Timestamp> seen;
    while (stream->next(event)) seen.push_back(event.base.ts);

    ASSERT_EQ(seen.size(), 6u);
    for (std::size_t i = 1; i < seen.size(); ++i) EXPECT_LT(seen[i - 1], seen[i]);
    EXPECT_EQ(stream->stats().backwards_stamps, 0u);
    EXPECT_EQ(stream->stats().files_read, 3u);
}

// A saturated pool is a retry, not a lost file.
TEST(BinanceStream, SaturatedPoolIsRetried) {
    FakeFetchPool pool;
    pool.saturate(true);
    auto stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(pool.in_flight(), 0u);

    pool.saturate(false);
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(pool.in_flight(), 2u);
}

TEST(BinanceStream, ParsesDeliveredFileIntoEvents) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(kTwoRowFile));

    ASSERT_TRUE(stream->next(event));
    EXPECT_EQ(event.base.kind, EventKind::Kline);
    EXPECT_EQ(event.base.exchange, kInstrument.exchange);
    EXPECT_EQ(event.base.market, kInstrument.market);
    EXPECT_EQ(event.base.symbol, kInstrument.symbol);
    EXPECT_EQ(event.base.ts, 1704067200000LL * 1'000'000);
    EXPECT_DOUBLE_EQ(std::get<KlineEvent>(event.payload).open, 42000.10);

    ASSERT_TRUE(stream->next(event));
    EXPECT_EQ(event.base.ts, 1704070800000LL * 1'000'000);
}

TEST(BinanceStream, HeaderRowIsCountedNotParsed) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(kTwoRowFile));
    drain(*stream);

    EXPECT_EQ(stream->stats().rows_parsed, 2u);
    EXPECT_EQ(stream->stats().header_rows, 1u);
}

// Older files carry no header, so a skip here would be a dropped data row.
TEST(BinanceStream, HeaderlessFileSkipsNothing) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(
        "1704067200000,42000.10,42100.00,41900.00,42050.00,10.5,1704070799999,0,0,0,0,0\n"
        "1704070800000,42050.00,42200.00,42000.00,42150.00,11.5,1704074399999,0,0,0,0,0\n"));
    drain(*stream);

    EXPECT_EQ(stream->stats().rows_parsed, 2u);
    EXPECT_EQ(stream->stats().header_rows, 0u);
}

TEST(BinanceStream, MalformedRowsCountAsRejected) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(
        "1704067200000,42000.10,42100.00,41900.00,42050.00,10.5,1704070799999,0,0,0,0,0\n"
        "1704070800000,nope\n"));
    drain(*stream);

    EXPECT_EQ(stream->stats().rows_parsed, 1u);
    EXPECT_EQ(stream->stats().rows_rejected, 1u);
}

TEST(BinanceStream, DrainsAcrossSeveralFiles) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    int         count = 0;
    for (int round = 0; round < 2; ++round) {
        while (stream->next(event)) ++count;
        ASSERT_TRUE(pool.deliver(kTwoRowFile));
        while (stream->next(event)) ++count;
    }

    EXPECT_EQ(count, 4);
    EXPECT_EQ(stream->stats().files_read, 2u);
    EXPECT_TRUE(stream->finished());
}

// An in-flight fetch means the stream is empty, not done.
TEST(BinanceStream, NotFinishedWhileAFetchIsOutstanding) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    EXPECT_FALSE(stream->finished());
}

// A short file is the failure the header count cannot see.
TEST(BinanceStream, ExpectedRowsComesFromIntervalAndCadence) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(kTwoRowFile));
    drain(*stream);

    EXPECT_EQ(stream->stats().rows_expected, 24);
    EXPECT_EQ(stream->stats().rows_parsed, 2u);
}

TEST(BinanceStream, ExpectedRowsUsesDaysInMonthForMonthlyFiles) {
    FakeFetchPool pool;
    KlineStream   stream(pool, BinanceMarket::UsdM, Cadence::Monthly, "BTCUSDT", "1h", kInstrument);
    stream.plan({"data/futures/um/monthly/klines/BTCUSDT/1h/BTCUSDT-1h-2024-02.zip"}, kJan, kFeb);

    MarketEvent event;
    (void)stream.next(event);
    ASSERT_TRUE(pool.deliver(kTwoRowFile));
    drain(stream);

    // 2024 is a leap year, so February is 29 days.
    EXPECT_EQ(stream.stats().rows_expected, 29 * 24);
}

// Funding has no interval in its path, so the count cannot be derived.
TEST(BinanceStream, ExpectedRowsIsZeroWithoutAnInterval) {
    FakeFetchPool pool;
    FundingStream stream(pool, BinanceMarket::UsdM, Cadence::Monthly, "BTCUSDT", "", kInstrument);
    stream.plan({"data/futures/um/monthly/fundingRate/BTCUSDT/BTCUSDT-fundingRate-2024-01.zip"},
                kJan, kFeb);

    MarketEvent event;
    (void)stream.next(event);
    ASSERT_TRUE(pool.deliver(
        "calc_time,funding_interval_hours,last_funding_rate\n1704067200000,8,0.0001\n"));
    while (stream.next(event)) {
    }

    EXPECT_EQ(stream.stats().rows_expected, 0);
    EXPECT_EQ(stream.stats().rows_parsed, 1u);
}

// A failed fetch must still pop, or the stream waits on it forever.
TEST(BinanceStream, FailedFetchUnblocksTheStream) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(
        pool.deliver_failure(qp::data_source::source::exchange::binance::FetchStatus::NotFound));

    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(stream->stats().files_failed, 1u);
    EXPECT_EQ(stream->stats().files_read, 0u);
    // The next file was asked for, so the stream moved on.
    EXPECT_EQ(pool.in_flight(), 1u);
}

// The rows that file should have held still count, so the loss is visible.
TEST(BinanceStream, FailedFetchStillCountsExpectedRows) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(
        pool.deliver_failure(qp::data_source::source::exchange::binance::FetchStatus::ZipError));
    (void)stream->next(event);

    EXPECT_EQ(stream->stats().rows_expected, 24);
    EXPECT_EQ(stream->stats().rows_parsed, 0u);
}

TEST(BinanceStream, FailedFetchSkipsToTheNextReadableFile) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(
        pool.deliver_failure(qp::data_source::source::exchange::binance::FetchStatus::Cancelled));
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(kTwoRowFile));

    EXPECT_EQ(drain(*stream), 2);
    EXPECT_EQ(stream->stats().files_failed, 1u);
    EXPECT_EQ(stream->stats().files_read, 1u);
}

// The trailing newline must not read as a blank line, or every file scores one.
TEST(BinanceStream, TrailingNewlineIsNotABlankRow) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(kTwoRowFile));
    drain(*stream);

    EXPECT_EQ(stream->stats().blank_rows, 0u);
    EXPECT_EQ(stream->stats().rows_parsed, 2u);
}

// A blank line inside the data is always an anomaly, so it gets counted.
TEST(BinanceStream, BlankLinesAreCounted) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event;
    (void)stream->next(event);
    ASSERT_TRUE(pool.deliver(
        "1704067200000,42000.10,42100.00,41900.00,42050.00,10.5,1704070799999,0,0,0,0,0\n"
        "\n"
        "1704070800000,42050.00,42200.00,42000.00,42150.00,11.5,1704074399999,0,0,0,0,0\n"));
    drain(*stream);

    EXPECT_EQ(stream->stats().blank_rows, 1u);
    EXPECT_EQ(stream->stats().rows_parsed, 2u);
    EXPECT_EQ(stream->stats().rows_rejected, 0u);
}

// Driving next() without draining lets files pile up, so the window has to stop
// asking or a delivery lands on a slot still holding an unread file.
TEST(BinanceStream, SlotsNeverOverflow) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(long_daily_keys(20), kJan, kFeb);

    MarketEvent event;
    long long   open_ms = 1704067200000LL;
    for (int step = 0; step < 40; ++step) {
        (void)stream->next(event);
        while (pool.in_flight() > 0) {
            EXPECT_TRUE(pool.deliver(two_rows_from(open_ms))) << "dropped at " << step;
            open_ms += 86'400'000LL;
        }
    }
}

TEST(BinanceStream, EmptyPlanIsFinishedAndAsksForNothing) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kFeb, kFeb);

    MarketEvent event;
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(stream->planned_files(), 0u);
    EXPECT_EQ(pool.in_flight(), 0u);
    EXPECT_TRUE(stream->finished());
}

// The early USD-M metrics files carry every row twice, byte identical. That is
// real published data, so it is counted and still emitted, not dropped. A
// dataset like aggTrades shares a stamp across genuinely distinct events, so
// dropping on a repeat could never be a stream wide rule.
TEST(BinanceStream, RepeatedStampsAreCountedNotDropped) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(duplicated_rows_from(1704067200000LL)));

    std::vector<qp::Timestamp> seen;
    while (stream->next(event)) seen.push_back(event.base.ts);

    ASSERT_EQ(seen.size(), 4u);
    EXPECT_EQ(seen[0], seen[1]);
    EXPECT_EQ(seen[2], seen[3]);
    EXPECT_EQ(stream->stats().repeated_stamps, 2u);
    EXPECT_EQ(stream->stats().backwards_stamps, 0u);
    EXPECT_EQ(stream->stats().rows_parsed, 4u);
}

TEST(BinanceStream, DistinctStampsCountNoRepeats) {
    FakeFetchPool pool;
    auto          stream = make_kline_stream(pool);
    stream->plan(daily_keys(), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(two_rows_from(1704067200000LL)));
    while (stream->next(event)) {
    }

    EXPECT_EQ(stream->stats().repeated_stamps, 0u);
}

// ---------------------------------------------------------------------------
// Grouped rows. bookDepth writes ten rows per sample, so the stream collects
// each run sharing a leading field and hands it to the parser whole.
// ---------------------------------------------------------------------------

namespace {

std::vector<std::string> depth_keys(int days) {
    std::vector<std::string> keys;
    for (int day = 1; day <= days; ++day) {
        const std::string stamp = (day < 10 ? "0" : "") + std::to_string(day);
        keys.push_back("data/futures/um/daily/bookDepth/BTCUSDT/BTCUSDT-bookDepth-2024-01-" +
                       stamp + ".zip");
    }
    return keys;
}

std::unique_ptr<DepthStream> make_depth_stream(FakeFetchPool& pool) {
    return std::make_unique<DepthStream>(pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "",
                                         kInstrument);
}

/// The rows of one sample at 2024-01-01 00:00:<second>, restricted to the
/// given bands so a test can build short, long or duplicated groups.
std::string depth_rows(int                     second,
                       const std::vector<int>& bands = {-5, -4, -3, -2, -1, 1, 2, 3, 4, 5}) {
    std::string out;
    for (auto band : bands) {
        out += "2024-01-01 00:00:";
        out += second < 10 ? "0" : "";
        out += std::to_string(second);
        out += ',';
        out += std::to_string(band);
        out += ",1";
        out += std::to_string(band < 0 ? -band : band);
        out += ".5,2";
        out += std::to_string(band < 0 ? -band : band);
        out += ".25\n";
    }
    return out;
}

const std::string kDepthHeader = "timestamp,percentage,depth,notional\n";

constexpr qp::Timestamp kSecond = 1'000'000'000LL;

std::vector<MarketEvent> events_of(DepthStream& stream) {
    std::vector<MarketEvent> out;
    MarketEvent              event{};
    while (stream.next(event)) out.push_back(event);
    return out;
}

}  // namespace

TEST(BinanceStreamGrouped, TenRowsMakeOneEvent) {
    FakeFetchPool pool;
    auto          stream = make_depth_stream(pool);
    stream->plan(depth_keys(1), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(kDepthHeader + depth_rows(10) + depth_rows(40)));

    const auto events = events_of(*stream);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].base.kind, EventKind::BookDepth);
    EXPECT_EQ(events[0].base.ts, kJan + 10 * kSecond);
    EXPECT_EQ(events[1].base.ts, kJan + 40 * kSecond);

    const auto* depth = std::get_if<qp::BookDepthEvent>(&events[0].payload);
    ASSERT_NE(depth, nullptr);
    ASSERT_NE(depth->bands, nullptr);
    EXPECT_DOUBLE_EQ(depth->bands->bids[4].depth, 15.5);
    EXPECT_DOUBLE_EQ(depth->bands->asks[0].notional, 21.25);

    EXPECT_EQ(stream->stats().rows_parsed, 20u) << "rows, not events";
    EXPECT_EQ(stream->stats().rows_rejected, 0u);
    EXPECT_EQ(stream->stats().header_rows, 1u);
}

// A short group is rejected as a whole and counted by its rows, and the next
// group still parses.
TEST(BinanceStreamGrouped, ShortGroupRejectedInFull) {
    FakeFetchPool pool;
    auto          stream = make_depth_stream(pool);
    stream->plan(depth_keys(1), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(depth_rows(10, {-5, -4, -3, -2, -1, 1, 2, 3, 4}) + depth_rows(40)));

    const auto events = events_of(*stream);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].base.ts, kJan + 40 * kSecond);
    EXPECT_EQ(stream->stats().rows_rejected, 9u);
    EXPECT_EQ(stream->stats().rows_parsed, 10u);
}

// An oversized run is consumed whole, not split into a full group and a stray
// row that would then be read as the start of the next one.
TEST(BinanceStreamGrouped, OversizedGroupRejectedInFull) {
    FakeFetchPool pool;
    auto          stream = make_depth_stream(pool);
    stream->plan(depth_keys(1), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(
        pool.deliver(depth_rows(10, {-5, -4, -3, -2, -1, 1, 2, 3, 4, 5, 5}) + depth_rows(40)));

    const auto events = events_of(*stream);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].base.ts, kJan + 40 * kSecond);
    EXPECT_EQ(stream->stats().rows_rejected, 11u);
}

// The run ends with the file, so half a sample at the end of one file and half
// at the start of the next are two bad groups, never one fused good one.
TEST(BinanceStreamGrouped, GroupNeverSpansFiles) {
    FakeFetchPool pool;
    auto          stream = make_depth_stream(pool);
    stream->plan(depth_keys(2), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(depth_rows(10, {-5, -4, -3, -2, -1})));
    ASSERT_TRUE(pool.deliver(depth_rows(10, {1, 2, 3, 4, 5}) + depth_rows(40)));

    const auto events = events_of(*stream);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].base.ts, kJan + 40 * kSecond);
    EXPECT_EQ(stream->stats().rows_rejected, 10u);
    EXPECT_EQ(stream->stats().files_read, 2u);
}

// A blank line ends a run like any other change of leading field.
TEST(BinanceStreamGrouped, BlankLineEndsGroup) {
    FakeFetchPool pool;
    auto          stream = make_depth_stream(pool);
    stream->plan(depth_keys(1), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(depth_rows(10) + "\n" + depth_rows(40)));

    EXPECT_EQ(events_of(*stream).size(), 2u);
    EXPECT_EQ(stream->stats().blank_rows, 1u);
}

TEST(BinanceStreamGrouped, CrlfRowsGroup) {
    FakeFetchPool pool;
    auto          stream = make_depth_stream(pool);
    stream->plan(depth_keys(1), kJan, kFeb);

    std::string file = depth_rows(10);
    std::string crlf;
    for (char c : file) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(crlf));
    EXPECT_EQ(events_of(*stream).size(), 1u);
    EXPECT_EQ(stream->stats().rows_rejected, 0u);
}

// The stamp is the sixth field of an aggTrades row and many trades share a
// millisecond, so the stream has to order on the right column and count those
// shared stamps without dropping them.
TEST(BinanceStream, AggTradesStampsFromTheSixthFieldAndKeepSharedMilliseconds) {
    FakeFetchPool pool;
    auto          stream = std::make_unique<TradesStream>(pool, BinanceMarket::UsdM, Cadence::Daily,
                                                          "BTCUSDT", "", kInstrument);
    stream->plan({"data/futures/um/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2024-01-01.zip"}, kJan,
                 kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(
        "agg_trade_id,price,quantity,first_trade_id,last_trade_id,transact_time,is_buyer_maker\n"
        "9,42000.1,0.5,100,100,1704067200000,false\n"
        "10,42000.2,0.1,101,103,1704067200000,true\n"
        "11,42000.0,0.2,104,104,1704067200001,false\n"));

    std::vector<MarketEvent> events;
    while (stream->next(event)) events.push_back(event);

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].base.kind, EventKind::Trade);
    EXPECT_EQ(events[0].base.ts, 1704067200000LL * 1'000'000);
    EXPECT_EQ(events[1].base.ts, events[0].base.ts);
    EXPECT_EQ(events[2].base.ts, 1704067200001LL * 1'000'000);
    EXPECT_EQ(std::get<qp::TradeEvent>(events[1].payload).side, qp::Side::Sell);

    EXPECT_EQ(stream->stats().rows_parsed, 3u);
    EXPECT_EQ(stream->stats().repeated_stamps, 1u);
    EXPECT_EQ(stream->stats().backwards_stamps, 0u);
    EXPECT_EQ(stream->stats().header_rows, 1u);
    EXPECT_EQ(stream->stats().sequence_gaps, 0u) << "ids 9, 10, 11 run on";
}

namespace {

/// Headerless futures rows with the given aggregate ids, a millisecond apart.
std::string trades_with_ids(const std::vector<long long>& ids) {
    std::string out;
    long long   ms = 1704067200000LL;
    for (auto id : ids) {
        out += std::to_string(id);
        out += ",42000.1,0.5,";
        out += std::to_string(id * 10);
        out += ',';
        out += std::to_string(id * 10);
        out += ',';
        out += std::to_string(ms++);
        out += ",false\n";
    }
    return out;
}

std::vector<std::string> trades_keys(int days) {
    std::vector<std::string> keys;
    for (int day = 1; day <= days; ++day)
        keys.push_back("data/futures/um/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2024-01-0" +
                       std::to_string(day) + ".zip");
    return keys;
}

std::unique_ptr<TradesStream> make_trades_stream(FakeFetchPool& pool) {
    return std::make_unique<TradesStream>(pool, BinanceMarket::UsdM, Cadence::Daily, "BTCUSDT", "",
                                          kInstrument);
}

std::size_t drain_trades(TradesStream& stream) {
    std::size_t n = 0;
    MarketEvent event{};
    while (stream.next(event)) ++n;
    return n;
}

}  // namespace

// A jump in the aggregate ids is tape the file does not have. Every event still
// comes through, and the break is counted.
TEST(BinanceStreamSequence, JumpInIdsCountsOneGap) {
    FakeFetchPool pool;
    auto          stream = make_trades_stream(pool);
    stream->plan(trades_keys(1), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(trades_with_ids({9, 10, 13, 14})));

    EXPECT_EQ(drain_trades(*stream), 4u);
    EXPECT_EQ(stream->stats().sequence_gaps, 1u);
}

// Ids carry on across files, so a clean day boundary is not a gap.
TEST(BinanceStreamSequence, IdsRunOnAcrossFiles) {
    FakeFetchPool pool;
    auto          stream = make_trades_stream(pool);
    stream->plan(trades_keys(2), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(trades_with_ids({1, 2, 3})));
    ASSERT_TRUE(pool.deliver(trades_with_ids({4, 5})));

    EXPECT_EQ(drain_trades(*stream), 5u);
    EXPECT_EQ(stream->stats().sequence_gaps, 0u);
}

// A day that fails to fetch leaves the next day's first id far past the last
// one read, which is exactly the tape that went missing.
TEST(BinanceStreamSequence, FailedFileShowsAsAGap) {
    FakeFetchPool pool;
    auto          stream = make_trades_stream(pool);
    stream->plan(trades_keys(3), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(trades_with_ids({1, 2})));
    ASSERT_TRUE(
        pool.deliver_failure(qp::data_source::source::exchange::binance::FetchStatus::NotFound));
    ASSERT_TRUE(pool.deliver(trades_with_ids({5, 6})));

    EXPECT_EQ(drain_trades(*stream), 4u);
    EXPECT_EQ(stream->stats().files_failed, 1u);
    EXPECT_EQ(stream->stats().sequence_gaps, 1u);
}

// A repeated id is not the next one either, so it counts rather than passing
// as continuity.
TEST(BinanceStreamSequence, RepeatedIdCountsAsAGap) {
    FakeFetchPool pool;
    auto          stream = make_trades_stream(pool);
    stream->plan(trades_keys(1), kJan, kFeb);

    MarketEvent event{};
    EXPECT_FALSE(stream->next(event));
    ASSERT_TRUE(pool.deliver(trades_with_ids({1, 2, 2, 3})));

    EXPECT_EQ(drain_trades(*stream), 4u);
    EXPECT_EQ(stream->stats().sequence_gaps, 1u)
        << "the second 2 breaks the run, the 3 after it does not";
}
