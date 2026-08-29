#include <gtest/gtest.h>

#include <memory>

#include "backtest_in_process_transport.hpp"
#include "control_channel.hpp"
#include "spmc_ring.hpp"

using qp::ControlChannel;
using qp::ControlCommand;
using qp::MarketEvent;
using qp::SpmcRing;
using qp::engine::transport::BacktestInProcessTransport;

namespace {

using Ring = SpmcRing<std::shared_ptr<const MarketEvent>, 8, 1>;

constexpr qp::Timestamp kSentinel = 999999;

MarketEvent trade(qp::Timestamp ts, qp::SymbolId symbol) {
    MarketEvent ev;
    ev.ts     = ts;
    ev.symbol = symbol;
    return ev;
}

void push(Ring& r, qp::Timestamp ts, qp::SymbolId symbol) {
    r.push(std::make_shared<const MarketEvent>(trade(ts, symbol)));
}

// One ring, one consumer, never stopped — minimal ControlChannel every test needs.
struct Harness {
    ControlChannel<1> control;
    std::size_t       idx = control.attach();
};

}  // namespace

TEST(BacktestInProcessTransport, OrdersByTimestampNotArrivalOrder) {
    Ring a, b;
    push(b, 200, 2);
    push(b, 300, 2);
    push(b, kSentinel, 2);
    push(a, 100, 1);
    push(a, 400, 1);
    push(a, kSentinel, 1);

    Harness                                h;
    BacktestInProcessTransport<Ring, 2, 1> t({&a, &b}, {0, 0}, h.control, h.idx);

    for (qp::Timestamp expected : {100, 200, 300, 400}) {
        auto out = t.next();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->ts, expected);
    }
}

TEST(BacktestInProcessTransport, WaitsUntilEveryLegHasSomethingBuffered) {
    Ring a, b;
    push(a, 50, 1);

    Harness                                h;
    BacktestInProcessTransport<Ring, 2, 1> t({&a, &b}, {0, 0}, h.control, h.idx);

    EXPECT_FALSE(t.next().has_value());  // b empty -> can't know if its next event sorts earlier

    push(b, 999, 2);
    auto out = t.next();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->ts, 50);
}

TEST(BacktestInProcessTransport, StallsRatherThanGuessingAnEmptyRingIsDone) {
    Ring a, b;
    push(a, 10, 1);
    push(b, 20, 2);  // only one event, no follow-up

    Harness                                h;
    BacktestInProcessTransport<Ring, 2, 1> t({&a, &b}, {0, 0}, h.control, h.idx);

    ASSERT_TRUE(t.next().has_value());
    EXPECT_FALSE(
        t.next().has_value());  // b's ring now empty -> can't safely emit a's next event yet
}

TEST(BacktestInProcessTransport, TiesBreakToTheLowestRingIndex) {
    Ring a, b;
    push(a, 500, 1);
    push(b, 500, 2);

    Harness                                h;
    BacktestInProcessTransport<Ring, 2, 1> t({&a, &b}, {0, 0}, h.control, h.idx);

    auto out = t.next();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->symbol, 1u);
}

TEST(BacktestInProcessTransport, InterleavesThreeLegsInTimestampOrder) {
    Ring a, b, c;
    push(a, 10, 0);
    push(a, 40, 0);
    push(a, kSentinel, 0);
    push(b, 20, 1);
    push(b, 50, 1);
    push(b, kSentinel, 1);
    push(c, 30, 2);
    push(c, 60, 2);
    push(c, kSentinel, 2);

    Harness                                h;
    BacktestInProcessTransport<Ring, 3, 1> t({&a, &b, &c}, {0, 0, 0}, h.control, h.idx);

    for (qp::Timestamp expected : {10, 20, 30, 40, 50, 60}) {
        auto out = t.next();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->ts, expected);
    }
}

TEST(BacktestInProcessTransport, StopFlushesBufferedEventsInsteadOfStallingForever) {
    Ring a, b;
    push(a, 10, 1);
    push(a, 30, 1);
    push(b, 20, 2);  // only one event -> b's ring goes empty after the first round

    ControlChannel<1>                      control;
    auto                                   idx = control.attach();
    BacktestInProcessTransport<Ring, 2, 1> t({&a, &b}, {0, 0}, control, idx);

    EXPECT_EQ(t.next()->ts, 10);  // fills a=10,b=20 -> a's 10 wins
    EXPECT_EQ(t.next()->ts,
              20);  // refills a=30 (its 2nd event); b's already-buffered 20 still wins
    EXPECT_FALSE(
        t.next().has_value());  // a's buffered 30 remains; b's ring empty -> stall, not stopped yet
    EXPECT_FALSE(t.is_done());

    // broadcast() directly, not request_stop(): request_stop() only reaches
    // poll() via ControlChannel's own background pump thread now (~100ms
    // cadence, control_channel.hpp) — that round trip is ControlChannel's
    // own concern (test_control_channel.cpp), not this transport's. This
    // test is about next()'s flush behavior once Stop is observed, so
    // broadcast() (still synchronous, bypasses the pump) is what actually
    // isolates that.
    control.broadcast(ControlCommand::Stop);
    EXPECT_FALSE(t.is_done());  // observed by next() below, not yet
    auto flushed = t.next();
    ASSERT_TRUE(flushed.has_value());
    EXPECT_EQ(flushed->ts, 30);  // stopped -> flush a's buffered 30
    // b's lookahead was already empty going into this call (its ring drained earlier),
    // so returning a's last buffered value also leaves everything fully drained now.
    EXPECT_TRUE(t.is_done());
    EXPECT_FALSE(t.next().has_value());  // truly nothing left anywhere
    EXPECT_TRUE(t.is_done());
}
