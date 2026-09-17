#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <thread>

#include "backtest_in_process_transport.hpp"
#include "spmc_queue.hpp"
#include "support/market_event_builders.hpp"

using qp::MarketEvent;
using qp::SlotOffset;
using qp::SpmcQueue;
using qp::engine::transport::BacktestInProcessTransport;

namespace {

constexpr std::size_t kCapacity = 8;
using Transport                 = BacktestInProcessTransport<kCapacity, 2>;
using Queue                     = Transport::Queue;

constexpr qp::Timestamp kSentinel = 999999;

MarketEvent trade(qp::Timestamp ts, SlotOffset symbol) {
    return qp::test::make_trade(symbol, ts, 0.0);
}

void push(Queue& q, qp::Timestamp ts, SlotOffset symbol) { q.push(trade(ts, symbol)); }

}  // namespace

TEST(BacktestInProcessTransport, OrdersByTimestampNotArrivalOrder) {
    Queue a, b;
    push(b, 200, 2);
    push(b, 300, 2);
    push(b, kSentinel, 2);
    push(a, 100, 1);
    push(a, 400, 1);
    push(a, kSentinel, 1);

    Transport t({&a, &b}, {0, 0});

    for (qp::Timestamp expected : {100, 200, 300, 400}) {
        auto out = t.next();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->ts, expected);
    }
}

TEST(BacktestInProcessTransport, WaitsUntilEveryLegHasSomethingBuffered) {
    Queue a, b;
    push(a, 50, 1);

    Transport t({&a, &b}, {0, 0});

    EXPECT_FALSE(t.next().has_value());

    push(b, 999, 2);
    auto out = t.next();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->ts, 50);
}

TEST(BacktestInProcessTransport, StallsRatherThanGuessingAnEmptyLegIsDone) {
    Queue a, b;
    push(a, 10, 1);
    push(b, 20, 2);

    Transport t({&a, &b}, {0, 0});

    ASSERT_TRUE(t.next().has_value());
    EXPECT_FALSE(t.next().has_value());
}

TEST(BacktestInProcessTransport, TiesBreakToTheLowestQueueIndex) {
    Queue a, b;
    push(a, 500, 1);
    push(b, 500, 2);

    Transport t({&a, &b}, {0, 0});

    auto out = t.next();
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->event->base.symbol, 1u);
}

TEST(BacktestInProcessTransport, InterleavesThreeLegsInTimestampOrder) {
    using Transport3 = BacktestInProcessTransport<kCapacity, 3>;
    Queue a, b, c;
    push(a, 10, 0);
    push(a, 40, 0);
    push(a, kSentinel, 0);
    push(b, 20, 1);
    push(b, 50, 1);
    push(b, kSentinel, 1);
    push(c, 30, 2);
    push(c, 60, 2);
    push(c, kSentinel, 2);

    Transport3 t({&a, &b, &c}, {0, 0, 0});

    for (qp::Timestamp expected : {10, 20, 30, 40, 50, 60}) {
        auto out = t.next();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->ts, expected);
    }
}

TEST(BacktestInProcessTransport, FlushDrainsBufferedEventsInsteadOfStallingOnAnEmptyLeg) {
    Queue a, b;
    push(a, 10, 1);
    push(a, 30, 1);
    push(b, 20, 2);

    Transport t({&a, &b}, {0, 0});

    EXPECT_EQ(t.next()->ts, 10);
    EXPECT_EQ(t.next()->ts, 20);
    EXPECT_FALSE(t.next().has_value());

    t.flush();
    auto flushed = t.next();
    ASSERT_TRUE(flushed.has_value());
    EXPECT_EQ(flushed->ts, 30);
    EXPECT_FALSE(t.next().has_value());
}

TEST(BacktestInProcessTransport, FlushEmittedInTimestampOrderAcrossLegs) {
    Queue a, b;
    push(a, 10, 1);
    push(a, 40, 1);
    push(b, 20, 2);
    push(b, 30, 2);

    Transport t({&a, &b}, {0, 0});
    t.flush();

    for (qp::Timestamp expected : {10, 20, 30, 40}) {
        auto out = t.next();
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->ts, expected);
    }
    EXPECT_FALSE(t.next().has_value());
}

TEST(BacktestInProcessTransport, ConcurrentProducersMergeCorrectlyUnderRealThreads) {
    constexpr std::size_t N             = 2;
    constexpr int         kEventsPerLeg = 500;
    using SmallTransport                = BacktestInProcessTransport<64, N>;
    using SmallQueue                    = SmallTransport::Queue;

    std::array<SmallQueue, N>  queues;
    std::array<SmallQueue*, N> queue_ptrs{&queues[0], &queues[1]};

    SmallTransport t(queue_ptrs, {0, 0});

    std::array<std::thread, N> producers;
    for (std::size_t leg = 0; leg < N; ++leg) {
        producers[leg] = std::thread([&queues, leg] {
            for (int i = 0; i < kEventsPerLeg; ++i) {
                while (!queues[leg].push(trade(i, static_cast<SlotOffset>(leg)))) {
                }
            }
        });
    }
    std::atomic<bool> producers_done{false};
    std::thread       joiner([&] {
        for (auto& p : producers) p.join();
        producers_done.store(true, std::memory_order_release);
    });

    std::size_t   consumed = 0;
    qp::Timestamp last_ts  = -1;
    bool          flushed  = false;
    while (consumed < N * kEventsPerLeg) {
        if (!flushed && producers_done.load(std::memory_order_acquire)) {
            t.flush();
            flushed = true;
        }
        if (auto out = t.next()) {
            EXPECT_GE(out->ts, last_ts);
            last_ts = out->ts;
            ++consumed;
        }
    }

    joiner.join();
    EXPECT_EQ(consumed, N * kEventsPerLeg);
    EXPECT_FALSE(t.next().has_value());
}

TEST(BacktestInProcessTransport, InjectsTimerTicksOnPeriodBetweenEvents) {
    Queue a, b;
    push(a, 100, 1);
    push(a, 450, 1);
    push(a, kSentinel, 1);
    push(b, kSentinel, 2);

    Transport t({&a, &b}, {0, 0}, 100);

    auto first = t.next();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first->event.has_value());
    EXPECT_EQ(first->ts, 100);

    for (qp::Timestamp tick : {200, 300, 400}) {
        auto out = t.next();
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->event.has_value());
        EXPECT_EQ(out->ts, tick);
    }

    auto next_event = t.next();
    ASSERT_TRUE(next_event.has_value());
    ASSERT_TRUE(next_event->event.has_value());
    EXPECT_EQ(next_event->ts, 450);
}
