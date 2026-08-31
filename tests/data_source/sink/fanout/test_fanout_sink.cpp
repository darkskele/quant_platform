#include <gtest/gtest.h>

#include <memory>

#include "fanout_sink.hpp"
#include "sink.hpp"
#include "types.hpp"

using qp::MarketEvent;
using qp::data_source::sink::fanout::FanoutSink;

namespace {

MarketEvent trade(qp::Price price) {
    qp::TradeEvent ev;
    ev.price = price;
    return ev;
}

}  // namespace

static_assert(qp::data_source::sink::Sink<FanoutSink<4, 1>>);

TEST(FanoutSink, RecordPushesOntoTheRing) {
    FanoutSink<4, 1> sink;
    sink.record(trade(100.0));

    auto event = sink.ring().try_pop(0);
    ASSERT_TRUE(event.has_value());
    EXPECT_DOUBLE_EQ(std::get<qp::TradeEvent>(*event).price, 100.0);
}

TEST(FanoutSink, OneRecordFansOutToEveryConsumer) {
    FanoutSink<4, 3> sink;
    sink.record(trade(100.0));

    for (std::size_t consumer = 0; consumer < 3; ++consumer) {
        auto event = sink.ring().try_pop(consumer);
        ASSERT_TRUE(event.has_value());
        EXPECT_DOUBLE_EQ(std::get<qp::TradeEvent>(*event).price, 100.0);
    }
}

TEST(FanoutSink, BookDiffLevelsAreSharedNotDeepCopiedAcrossConsumers) {
    FanoutSink<4, 2> sink;
    auto             levels =
        std::make_shared<const qp::BookLevels>(qp::BookLevels{{{100.0, 1.0}}, {{101.0, 1.0}}});

    qp::BookDiffEvent ev;
    ev.levels = levels;
    sink.record(ev);

    auto first  = sink.ring().try_pop(0);
    auto second = sink.ring().try_pop(1);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    // Each consumer's popped copy shares the same underlying BookLevels —
    // a refcount bump, not a deep copy of the price levels.
    EXPECT_EQ(std::get<qp::BookDiffEvent>(*first).levels.get(), levels.get());
    EXPECT_EQ(std::get<qp::BookDiffEvent>(*second).levels.get(), levels.get());
}
