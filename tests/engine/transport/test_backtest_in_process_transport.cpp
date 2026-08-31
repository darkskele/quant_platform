#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <thread>

#include "backtest_in_process_transport.hpp"
#include "control_channel.hpp"
#include "spmc_ring.hpp"

using qp::ControlChannel;
using qp::ControlCommand;
using qp::MarketEvent;
using qp::SpmcRing;
using qp::engine::transport::BacktestInProcessTransport;

namespace {

using Ring = SpmcRing<MarketEvent, 8, 1>;

constexpr qp::Timestamp kSentinel = 999999;

MarketEvent trade(qp::Timestamp ts, qp::SymbolId symbol) {
    qp::TradeEvent ev;
    ev.ts     = ts;
    ev.symbol = symbol;
    return ev;
}

void push(Ring& r, qp::Timestamp ts, qp::SymbolId symbol) { r.push(trade(ts, symbol)); }

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
        EXPECT_EQ(header_of(*out).ts, expected);
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
    EXPECT_EQ(header_of(*out).ts, 50);
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
    EXPECT_EQ(header_of(*out).symbol, 1u);
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
        EXPECT_EQ(header_of(*out).ts, expected);
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

    EXPECT_EQ(header_of(*t.next()).ts, 10);  // fills a=10,b=20 -> a's 10 wins
    EXPECT_EQ(header_of(*t.next()).ts,
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
    EXPECT_EQ(header_of(*flushed).ts, 30);  // stopped -> flush a's buffered 30
    // b's lookahead was already empty going into this call (its ring drained earlier),
    // so returning a's last buffered value also leaves everything fully drained now.
    EXPECT_TRUE(t.is_done());
    EXPECT_FALSE(t.next().has_value());  // truly nothing left anywhere
    EXPECT_TRUE(t.is_done());
}

// Every test above pushes synchronously on one thread before calling
// next() — none of them exercise the real shape this class actually runs
// under: producer threads pushing into the rings concurrently with the
// consumer thread's next() calls. This spins up a real producer thread per
// leg (racing SpmcRing's own push()/try_pop()) and drains through
// next() on the main thread, for TSan to actually watch. Each leg pushes
// strictly increasing timestamps, so a global merge that's still correct
// under real interleaving produces a non-decreasing sequence overall —
// this checks that invariant, not just the absence of a reported race.
TEST(BacktestInProcessTransport, ConcurrentProducersMergeCorrectlyUnderRealThreads) {
    constexpr std::size_t N             = 2;
    constexpr int         kEventsPerLeg = 500;
    using Ring                          = SpmcRing<MarketEvent, 64, 1>;

    std::array<Ring, N>  rings;
    std::array<Ring*, N> ring_ptrs{&rings[0], &rings[1]};

    Harness                                h;
    BacktestInProcessTransport<Ring, N, 1> t(ring_ptrs, {0, 0}, h.control, h.idx);

    std::array<std::thread, N> producers;
    for (std::size_t leg = 0; leg < N; ++leg) {
        producers[leg] = std::thread([&rings, leg] {
            for (int i = 0; i < kEventsPerLeg; ++i) {
                rings[leg].push(trade(i, static_cast<qp::SymbolId>(leg)));
            }
        });
    }
    // Signals Stop once every producer is done — next() otherwise can't
    // distinguish "this ring is genuinely exhausted" from "hasn't produced
    // yet", same reason StopFlushesBufferedEventsInsteadOfStallingForever
    // above needs an explicit Stop rather than an empty ring alone.
    std::thread stop_signaler([&] {
        for (auto& producer : producers) producer.join();
        h.control.broadcast(ControlCommand::Stop);
    });

    std::size_t   consumed = 0;
    qp::Timestamp last_ts  = -1;
    while (consumed < N * kEventsPerLeg) {
        if (auto out = t.next()) {
            EXPECT_GE(header_of(*out).ts, last_ts);
            last_ts = header_of(*out).ts;
            ++consumed;
        }
    }

    stop_signaler.join();
    EXPECT_EQ(consumed, N * kEventsPerLeg);
    EXPECT_TRUE(t.is_done());
}
