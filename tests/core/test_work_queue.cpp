#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include "work_queue.hpp"

TEST(WorkQueue, PushTryPopFifoFullEmpty) {
    qp::WorkQueue<int, 4> q;  // all 4 slots usable, no wasted slot
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_TRUE(q.push(4));
    EXPECT_FALSE(q.push(5));  // full

    EXPECT_EQ(*q.try_pop(), 1);
    EXPECT_EQ(*q.try_pop(), 2);
    EXPECT_TRUE(q.push(5));  // popping two freed room for one more
    EXPECT_EQ(*q.try_pop(), 3);
    EXPECT_EQ(*q.try_pop(), 4);
    EXPECT_EQ(*q.try_pop(), 5);
    EXPECT_FALSE(q.try_pop().has_value());  // empty
}

TEST(WorkQueue, TryPopEmptyIsNullopt) {
    qp::WorkQueue<int, 4> q;
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(WorkQueue, PushNeverBlocksWhenFull) {
    qp::WorkQueue<int, 4> q;  // nothing ever popped
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(q.push(i));

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10'000; ++i) EXPECT_FALSE(q.push(i));
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::milliseconds(50));
}

// A slot is handed back to the producer one lap on, so a queue driven for
// many laps is what exercises that hand back.
TEST(WorkQueue, ManyLapsKeepFifoOrder) {
    constexpr int         Laps = 10'000;
    qp::WorkQueue<int, 4> q;

    for (int i = 0; i < Laps; ++i) {
        ASSERT_TRUE(q.push(i));
        ASSERT_TRUE(q.push(i + 1));
        ASSERT_EQ(*q.try_pop(), i);
        ASSERT_EQ(*q.try_pop(), i + 1);
    }
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(WorkQueue, HeapStorageBehavesTheSame) {
    qp::WorkQueue<int, 4, /*UseHeap=*/true> q;
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(q.push(i));
    EXPECT_FALSE(q.push(4));
    for (int i = 0; i < 4; ++i) EXPECT_EQ(*q.try_pop(), i);
    EXPECT_FALSE(q.try_pop().has_value());
}

namespace {
int g_alive = 0;

struct Tracked {
    int v;

    explicit Tracked(int x) : v(x) { ++g_alive; }

    Tracked(Tracked&& o) noexcept : v(o.v) { ++g_alive; }

    Tracked(const Tracked&) = delete;

    ~Tracked() { --g_alive; }
};
}  // namespace

TEST(WorkQueue, DestructorCleansUpRemainingElements) {
    g_alive = 0;
    {
        qp::WorkQueue<Tracked, 8> q;
        for (int i = 0; i < 5; ++i) EXPECT_TRUE(q.push(i));
        EXPECT_EQ(g_alive, 5);
        for (int i = 0; i < 3; ++i) EXPECT_EQ(q.try_pop()->v, i);
        EXPECT_EQ(g_alive, 2);
    }  // destructor must clean up the 2 remaining elements
    EXPECT_EQ(g_alive, 0);
}

// A claim moves the shared cursor before the element leaves the slot, so the
// destructor's live range is what a concurrent drain is most likely to break.
TEST(WorkQueue, DestructorCleansUpAfterConcurrentConsumers) {
    g_alive = 0;
    {
        qp::WorkQueue<Tracked, 8> q;
        for (int i = 0; i < 8; ++i) EXPECT_TRUE(q.push(i));

        // Six fixed pops across three consumers, so two elements are always
        // left for the destructor no matter who wins a race.
        std::vector<std::thread> consumers;
        for (int c = 0; c < 3; ++c)
            consumers.emplace_back([&] {
                for (int i = 0; i < 2; ++i)
                    while (!q.try_pop()) std::this_thread::yield();
            });
        for (auto& t : consumers) t.join();
        EXPECT_EQ(g_alive, 2);
    }
    EXPECT_EQ(g_alive, 0);
}

TEST(WorkQueue, ConcurrentConsumersTakeEachElementOnce) {
    constexpr int               N            = 200'000;
    constexpr std::size_t       NumConsumers = 6;
    qp::WorkQueue<int, 1 << 10> q;

    std::atomic<int>              taken{0};
    std::vector<std::vector<int>> received(NumConsumers);
    std::vector<std::thread>      consumers;
    for (std::size_t c = 0; c < NumConsumers; ++c) {
        received[c].reserve(N);
        consumers.emplace_back([&, c] {
            while (taken.load(std::memory_order_relaxed) < N) {
                if (auto v = q.try_pop()) {
                    received[c].push_back(*v);
                    taken.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::thread producer([&] {
        for (int i = 0; i < N; ++i)
            while (!q.push(i)) std::this_thread::yield();
    });

    producer.join();
    for (auto& t : consumers) t.join();

    // A consumer claims strictly increasing positions, and the value pushed at
    // a position is that position, so what one consumer sees must ascend.
    for (std::size_t c = 0; c < NumConsumers; ++c) {
        for (std::size_t i = 1; i < received[c].size(); ++i)
            ASSERT_LT(received[c][i - 1], received[c][i]) << "consumer " << c << " went backwards";
    }

    std::vector<char> seen(N, 0);
    int               duplicates = 0;
    int               total      = 0;
    for (const auto& per_consumer : received)
        for (int v : per_consumer) {
            ASSERT_GE(v, 0);
            ASSERT_LT(v, N);
            if (seen[v]) ++duplicates;
            seen[v] = 1;
            ++total;
        }

    EXPECT_EQ(duplicates, 0);
    EXPECT_EQ(total, N);
    for (int i = 0; i < N; ++i) EXPECT_TRUE(seen[i]) << "missing value " << i;
}
