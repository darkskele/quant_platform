#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include "mpsc_queue.hpp"

TEST(MpscQueue, PushTryPopFifoFullEmpty) {
    qp::MpscQueue<int, 4> q;  // all 4 slots usable, no wasted slot
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

TEST(MpscQueue, TryPopEmptyIsNullopt) {
    qp::MpscQueue<int, 4> q;
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(MpscQueue, PushNeverBlocksWhenFull) {
    qp::MpscQueue<int, 4> q;  // nothing ever popped
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(q.push(i));

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10'000; ++i) EXPECT_FALSE(q.push(i));
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::milliseconds(50));
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

TEST(MpscQueue, DestructorCleansUpRemainingElements) {
    g_alive = 0;
    {
        qp::MpscQueue<Tracked, 8> q;
        for (int i = 0; i < 5; ++i) EXPECT_TRUE(q.push(i));
        EXPECT_EQ(g_alive, 5);
        for (int i = 0; i < 3; ++i) EXPECT_EQ(q.try_pop()->v, i);
        EXPECT_EQ(g_alive, 2);
    }  // destructor must clean up the 2 remaining elements
    EXPECT_EQ(g_alive, 0);
}

TEST(MpscQueue, ConcurrentProducersLoseNoElement) {
    constexpr int               PerProducer  = 50'000;
    constexpr std::size_t       NumProducers = 4;
    constexpr int               Total        = PerProducer * static_cast<int>(NumProducers);
    qp::MpscQueue<int, 1 << 10> q;

    // Producer p emits values p, p+NumProducers, p+2*NumProducers ... so
    // every emitted value across all producers is distinct.
    std::vector<std::thread> producers;
    for (std::size_t p = 0; p < NumProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < PerProducer; ++i) {
                int value = static_cast<int>(p) + i * static_cast<int>(NumProducers);
                while (!q.push(value)) std::this_thread::yield();
            }
        });
    }

    std::vector<char> seen(Total, 0);
    int               duplicates = 0;
    std::thread       consumer([&] {
        for (int count = 0; count < Total;) {
            if (auto v = q.try_pop()) {
                if (seen[*v]) ++duplicates;
                seen[*v] = 1;
                ++count;
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();

    EXPECT_EQ(duplicates, 0);
    for (int i = 0; i < Total; ++i) EXPECT_TRUE(seen[i]) << "missing value " << i;
}
