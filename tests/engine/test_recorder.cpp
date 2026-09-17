#include <gtest/gtest.h>

#include "exchange.hpp"
#include "portfolio.hpp"
#include "recorder/equity_series/equity_series_recorder.hpp"
#include "recorder/null/null_recorder.hpp"
#include "recorder/recorder.hpp"
#include "subscription.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::engine::EquitySeriesCollector;
using qp::engine::EquitySeriesRecorder;
using qp::engine::NullRecorder;
using qp::engine::Recorder;

namespace {
constexpr std::uint16_t kExchange = static_cast<std::uint16_t>(ExchangeId::Binance);
constexpr std::uint16_t kMarket   = 0;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    return std::move(sub).build();
}

using Book = qp::Portfolio;
}  // namespace

static_assert(Recorder<NullRecorder, Book>);
static_assert(Recorder<EquitySeriesRecorder, Book>);

TEST(NullRecorder, SampleIsNoOp) {
    NullRecorder rec;
    Book         book{make_subscription()};
    rec.sample(1, book);
    SUCCEED();
}

TEST(EquitySeriesRecorder, CollectsSamplesInOrder) {
    EquitySeriesCollector collector;
    collector.start();
    EquitySeriesRecorder rec{&collector};
    Book                 book{make_subscription()};

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
    Book                 book{make_subscription()};

    rec.sample(1, book);
    book.apply_fill(qp::Fill{.exchange = kExchange,
                             .market   = kMarket,
                             .symbol   = 0,
                             .side     = qp::Side::Buy,
                             .price    = 100.0,
                             .qty      = 1.0,
                             .fee      = 0.5});
    book.apply_mark_price(qp::MarketEvent{.base    = {.kind     = qp::EventKind::Trade,
                                                      .exchange = kExchange,
                                                      .market   = kMarket,
                                                      .symbol   = 0},
                                          .payload = qp::TradeEvent{.price = 100.0}});
    rec.sample(2, book);
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
    Book                 book{make_subscription()};

    constexpr int N = 200'000;
    for (int i = 0; i < N; ++i) rec.sample(i, book);
    collector.finish();

    auto series = collector.series();
    ASSERT_EQ(series.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) EXPECT_EQ(series[i].ts, i);
}
