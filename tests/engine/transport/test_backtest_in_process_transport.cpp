#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <thread>

#include "backtest_in_process_transport.hpp"
#include "spmc_queue.hpp"

using qp::MarketEvent;
using qp::SpmcQueue;
using qp::engine::transport::BacktestInProcessTransport;

namespace {

constexpr std::size_t kCapacity = 8;
using Transport                 = BacktestInProcessTransport<kCapacity, 2>;
using Queue                     = Transport::Queue;

constexpr qp::Timestamp kSentinel = 999999;

MarketEvent trade(qp::Timestamp ts, qp::SymbolId symbol) {
    qp::TradeEvent ev;
    ev.ts     = ts;
    ev.symbol = symbol;
    return ev;
}

void push(Queue& q, qp::Timestamp ts, qp::SymbolId symbol) { q.push(trade(ts, symbol)); }

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
        EXPECT_EQ(header_of(*out).ts, expected);
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
    EXPECT_EQ(header_of(*out).ts, 50);
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
    EXPECT_EQ(header_of(*out).symbol, 1u);
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
        EXPECT_EQ(header_of(*out).ts, expected);
    }
}

TEST(BacktestInProcessTransport, FlushDrainsBufferedEventsInsteadOfStallingOnAnEmptyLeg) {
    Queue a, b;
    push(a, 10, 1);
    push(a, 30, 1);
    push(b, 20, 2);

    Transport t({&a, &b}, {0, 0});

    EXPECT_EQ(header_of(*t.next()).ts, 10);
    EXPECT_EQ(header_of(*t.next()).ts, 20);
    EXPECT_FALSE(t.next().has_value());

    t.flush();
    auto flushed = t.next();
    ASSERT_TRUE(flushed.has_value());
    EXPECT_EQ(header_of(*flushed).ts, 30);
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
        EXPECT_EQ(header_of(*out).ts, expected);
    }
    EXPECT_FALSE(t.next().has_value());
}

// A producer thread per leg pushing concurrently with the consumer's next()
// calls, for TSan to watch. Each leg pushes strictly increasing timestamps,
// so a correct merge is non-decreasing overall. flush() runs on the consumer
// thread once a coordinator signals every producer is done — otherwise next()
// would stall forever on a drained-but-still-live leg at the tail.
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
                while (!queues[leg].push(trade(i, static_cast<qp::SymbolId>(leg)))) {
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
            EXPECT_GE(header_of(*out).ts, last_ts);
            last_ts = header_of(*out).ts;
            ++consumed;
        }
    }

    joiner.join();
    EXPECT_EQ(consumed, N * kEventsPerLeg);
    EXPECT_FALSE(t.next().has_value());
}
