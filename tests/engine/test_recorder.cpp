#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include "portfolio.hpp"
#include "recorder/equity_series/equity_series_recorder.hpp"
#include "recorder/null/null_recorder.hpp"
#include "recorder/recorder.hpp"
#include "types.hpp"

using qp::engine::EquitySeriesCollector;
using qp::engine::EquitySeriesRecorder;
using qp::engine::NullRecorder;
using qp::engine::Recorder;

namespace {
constexpr std::array<std::size_t, 1> kCounts{1};
using Book = qp::Portfolio;
}  // namespace

static_assert(Recorder<NullRecorder, Book>);
static_assert(Recorder<EquitySeriesRecorder, Book>);

TEST(NullRecorder, SampleIsNoOp) {
    NullRecorder rec;
    Book         book{kCounts};
    rec.sample(1, book);
    SUCCEED();
}

TEST(EquitySeriesRecorder, CollectsSamplesInOrder) {
    EquitySeriesCollector collector;
    collector.start();
    EquitySeriesRecorder rec{&collector};
    Book                 book{kCounts};

    rec.sample(10, book);
    rec.sample(20, book);
    collector.finish();

    auto series = collector.series();
    ASSERT_EQ(series.size(), 2u);
    EXPECT_EQ(series[0].ts, 10);
    EXPECT_EQ(series[1].ts, 20);
}

TEST(EquitySeriesRecorder, CapturesEquityAtSampleTime) {
    EquitySeriesCollector collector;
    collector.start();
    EquitySeriesRecorder rec{&collector};
    Book                 book{kCounts};

    rec.sample(1, book);  // flat: equity 0
    book.apply_fill(
        qp::Fill{.symbol = 0, .side = qp::Side::Buy, .price = 100.0, .qty = 1.0, .fee = 0.5});
    book.apply_mark_price(qp::MarketEvent{
        qp::TradeEvent{.base = {.kind = qp::EventKind::Trade, .symbol = 0}, .price = 100.0}});
    rec.sample(2, book);  // cash -100.5, position 1 @ 100 -> equity -0.5
    collector.finish();

    auto series = collector.series();
    ASSERT_EQ(series.size(), 2u);
    EXPECT_DOUBLE_EQ(series[0].equity, 0.0);
    EXPECT_DOUBLE_EQ(series[1].equity, -0.5);
}

TEST(EquitySeriesRecorder, DrainsMoreSamplesThanTheQueueHoldsWithoutLoss) {
    EquitySeriesCollector collector;
    collector.start();
    EquitySeriesRecorder rec{&collector};
    Book                 book{kCounts};

    constexpr int N = 200'000;  // exceeds the internal queue, exercises backpressure
    for (int i = 0; i < N; ++i) rec.sample(i, book);
    collector.finish();

    auto series = collector.series();
    ASSERT_EQ(series.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) EXPECT_EQ(series[i].ts, i);
}
