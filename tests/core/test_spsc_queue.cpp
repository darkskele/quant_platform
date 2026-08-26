#include <gtest/gtest.h>

#include <thread>

#include "spsc_queue.hpp"

TEST(SpscQueue, PushPopFifoFullEmpty) {
    qp::SpscQueue<int, 4> q;  // capacity 4, one slot wasted -> 3 usable
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_TRUE(q.push(3));
    EXPECT_FALSE(q.push(4));  // full

    EXPECT_EQ(*q.pop(), 1);
    EXPECT_EQ(*q.pop(), 2);
    EXPECT_EQ(*q.pop(), 3);
    EXPECT_FALSE(q.pop().has_value());  // empty
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

TEST(SpscQueue, DestructorCleansUpRemainingElements) {
    g_alive = 0;
    {
        qp::SpscQueue<Tracked, 8> q;
        for (int i = 0; i < 5; ++i) EXPECT_TRUE(q.push(i));
        EXPECT_EQ(g_alive, 5);
        for (int i = 0; i < 3; ++i) EXPECT_EQ(q.pop()->v, i);
        EXPECT_EQ(g_alive, 2);
    }  // destructor must clean up the 2 remaining elements
    EXPECT_EQ(g_alive, 0);
}

TEST(SpscQueue, ThreadedProducerConsumerPreservesOrderAndCount) {
    constexpr int               N = 200'000;
    qp::SpscQueue<int, 1 << 12> q;

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            while (!q.push(i)) std::this_thread::yield();
        }
    });

    // GTest's ASSERT_*/EXPECT_* aren't meant to be evaluated off the main
    // test thread, so the consumer just records the first mismatch and the
    // test thread asserts on it after join().
    int         mismatch_at = -1, mismatch_got = -1;
    std::thread consumer([&] {
        for (int expected = 0; expected < N; ++expected) {
            std::optional<int> v;
            while (!(v = q.pop())) std::this_thread::yield();
            if (*v != expected) {
                mismatch_at  = expected;
                mismatch_got = *v;
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(mismatch_at, -1) << "expected " << mismatch_at << " got " << mismatch_got;
}
