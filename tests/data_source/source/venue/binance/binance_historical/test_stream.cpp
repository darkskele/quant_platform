#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "parsers/funding.hpp"
#include "parsers/klines.hpp"
#include "stream.hpp"
#include "support/fake_fetch_pool.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::venue::binance;

using binance::BinanceHistoricalStream;
using binance::BinanceMarket;
using binance::Cadence;
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
        char stamp[11];
        std::snprintf(stamp, sizeof(stamp), "2024-01-%02d", day);
        keys.push_back(std::string("data/futures/um/daily/klines/BTCUSDT/1h/BTCUSDT-1h-") + stamp +
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
    EXPECT_EQ(pool.in_flight(), binance::kFileSlotCount);

    EXPECT_FALSE(stream->next(event));
    EXPECT_FALSE(stream->next(event));
    EXPECT_EQ(pool.in_flight(), binance::kFileSlotCount) << "the window did not hold";
    EXPECT_EQ(pool.submitted().size(), binance::kFileSlotCount);
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
    EXPECT_EQ(pool.submitted().size(), binance::kFileSlotCount) << "delivery is not a read";

    ASSERT_TRUE(stream->next(event));
    EXPECT_EQ(pool.submitted().size(), binance::kFileSlotCount + 1);
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
        pool.deliver_failure(qp::data_source::source::venue::binance::FetchStatus::NotFound));

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
        pool.deliver_failure(qp::data_source::source::venue::binance::FetchStatus::ZipError));
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
        pool.deliver_failure(qp::data_source::source::venue::binance::FetchStatus::Cancelled));
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
