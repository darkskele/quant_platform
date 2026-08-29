#include <gtest/gtest.h>

#include <stdexcept>

#include "fanout_sink.hpp"
#include "sink.hpp"
#include "types.hpp"

using qp::EventKind;
using qp::MarketEvent;
using qp::data_source::sink::fanout::FanoutSink;

namespace {

MarketEvent trade(qp::Price price) {
    MarketEvent ev;
    ev.kind  = EventKind::Trade;
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
    EXPECT_DOUBLE_EQ((*event)->price, 100.0);
}

TEST(FanoutSink, AttachHandsOutSequentialIds) {
    FanoutSink<4, 2> sink;
    EXPECT_EQ(sink.attach(), 0u);
    EXPECT_EQ(sink.attach(), 1u);
    EXPECT_THROW(sink.attach(), std::out_of_range);
}
