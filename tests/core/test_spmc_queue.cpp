#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "spmc_queue.hpp"

TEST(SpmcQueue, PushTryPopFifoWithinCapacity) {
    qp::SpmcQueue<int, 4, 1> queue;
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));

    EXPECT_EQ(*queue.try_pop(0), 1);
    EXPECT_EQ(*queue.try_pop(0), 2);
    EXPECT_EQ(*queue.try_pop(0), 3);
    EXPECT_FALSE(queue.try_pop(0).has_value());  // caught up
}

TEST(SpmcQueue, PushSucceedsWithinCapacityThenFailsWhenFull) {
    qp::SpmcQueue<int, 4, 1> queue;
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(queue.push(i));  // virgin slots, always free

    // Consumer hasn't read anything: reusing slot 0 isn't free yet.
    EXPECT_FALSE(queue.push(4));

    EXPECT_EQ(*queue.try_pop(0), 0);
    EXPECT_TRUE(queue.push(4));  // freeing slot 0 makes room again

    for (int expected : {1, 2, 3, 4}) EXPECT_EQ(*queue.try_pop(0), expected);
}

TEST(SpmcQueue, PushNeverBlocksWhenFull) {
    qp::SpmcQueue<int, 4, 1> queue;  // no consumer ever reads
    for (int i = 0; i < 4; ++i) queue.push(i);

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10'000; ++i) EXPECT_FALSE(queue.push(i));
    auto elapsed = std::chrono::steady_clock::now() - start;

    // A blocking implementation would take milliseconds at least for
    // 10,000 attempts; this should be well under that.
    EXPECT_LT(elapsed, std::chrono::milliseconds(50));
}

TEST(SpmcQueue, EachConsumerSeesTheFullStreamIndependently) {
    qp::SpmcQueue<int, 8, 3> queue;
    for (int i = 0; i < 5; ++i) queue.push(i);  // < Capacity, single-threaded-safe

    for (int i = 0; i < 5; ++i) EXPECT_EQ(*queue.try_pop(0), i);
    EXPECT_FALSE(queue.try_pop(0).has_value());

    // Consumers 1 and 2 haven't read anything yet — still see the full
    // stream from the start, independent of consumer 0's progress.
    for (std::size_t c : {std::size_t{1}, std::size_t{2}}) {
        for (int i = 0; i < 5; ++i) EXPECT_EQ(*queue.try_pop(c), i);
        EXPECT_FALSE(queue.try_pop(c).has_value());
    }
}

namespace {
std::atomic<int> g_alive{0};

struct Tracked {
    int v;

    explicit Tracked(int x) : v(x) { ++g_alive; }

    Tracked(const Tracked& o) : v(o.v) { ++g_alive; }

    Tracked(Tracked&& o) noexcept : v(o.v) { ++g_alive; }

    // Assignment overwrites an existing live value in place.
    Tracked& operator=(const Tracked& o) noexcept {
        v = o.v;
        return *this;
    }

    Tracked& operator=(Tracked&& o) noexcept {
        v = o.v;
        return *this;
    }

    ~Tracked() { --g_alive; }
};
}  // namespace

TEST(SpmcQueue, DestructorCleansUpAllLiveSlotsNoWraparound) {
    g_alive = 0;
    {
        qp::SpmcQueue<Tracked, 8, 1> queue;
        for (int i = 0; i < 8; ++i) queue.push(i);  // fills exactly to Capacity, no wait
        EXPECT_EQ(g_alive.load(), 8);
    }  // destructor must destroy all 8 still-constructed slots
    EXPECT_EQ(g_alive.load(), 0);
}

TEST(SpmcQueue, WraparoundLeavesExactlyCapacityLiveAfterConsumerCatchesUp) {
    g_alive         = 0;
    constexpr int N = 50;
    {
        qp::SpmcQueue<Tracked, 8, 1> queue;

        std::thread consumer([&] {
            for (int i = 0; i < N; ++i) {
                std::optional<Tracked> v;
                while (!(v = queue.try_pop(0))) std::this_thread::yield();
                // v destroyed here, each iteration — no lingering copies
            }
        });

        for (int i = 0; i < N; ++i) {
            while (!queue.push(i)) std::this_thread::yield();
        }
        consumer.join();

        // push() never destroys on read — only the most recent Capacity
        // pushes are ever constructed at once, regardless of consumption.
        EXPECT_EQ(g_alive.load(), 8);
    }
    EXPECT_EQ(g_alive.load(), 0);
}

TEST(SpmcQueue, SlowConsumerNeverMissesAnEvent) {
    constexpr int            N = 40;
    qp::SpmcQueue<int, 4, 1> queue;

    std::vector<int> received;
    std::thread      consumer([&] {
        // Let the producer run well ahead and hit real push() failures
        // before this consumer reads anything.
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        for (int i = 0; i < N; ++i) {
            std::optional<int> v;
            while (!(v = queue.try_pop(0))) std::this_thread::yield();
            received.push_back(*v);
        }
    });

    std::size_t push_failures = 0;
    for (int i = 0; i < N; ++i) {
        while (!queue.push(i)) {
            ++push_failures;
            std::this_thread::yield();
        }
    }
    consumer.join();

    // The producer genuinely had to retry against the deliberately-delayed
    // consumer rather than racing ahead and silently overwriting unread
    // slots.
    EXPECT_GT(push_failures, 0u);

    ASSERT_EQ(received.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) EXPECT_EQ(received[i], i);
}

TEST(SpmcQueue, ManyConcurrentConsumersPreserveOrderAndCompleteness) {
    constexpr int                             N            = 50'000;
    constexpr std::size_t                     NumConsumers = 6;
    qp::SpmcQueue<int, 1 << 10, NumConsumers> queue;

    // GTest's ASSERT_*/EXPECT_* aren't meant to be evaluated off the main
    // test thread (same convention as SpscQueue's threaded test).
    std::vector<int>         mismatch_at(NumConsumers, -1);
    std::vector<int>         mismatch_got(NumConsumers, -1);
    std::vector<std::thread> consumers;
    for (std::size_t c = 0; c < NumConsumers; ++c) {
        consumers.emplace_back([&, c] {
            for (int expected = 0; expected < N; ++expected) {
                std::optional<int> v;
                while (!(v = queue.try_pop(c))) std::this_thread::yield();
                if (*v != expected) {
                    mismatch_at[c]  = expected;
                    mismatch_got[c] = *v;
                    break;
                }
            }
        });
    }

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            while (!queue.push(i)) std::this_thread::yield();
        }
    });

    producer.join();
    for (auto& t : consumers) t.join();

    for (std::size_t c = 0; c < NumConsumers; ++c) {
        EXPECT_EQ(mismatch_at[c], -1)
            << "consumer " << c << " expected " << mismatch_at[c] << " got " << mismatch_got[c];
    }
}

TEST(SpmcQueue, SharedPtrConsumersShareOwnershipNoDoubleFreeOrLeak) {
    g_alive                            = 0;
    constexpr int         N            = 200;
    constexpr std::size_t NumConsumers = 4;
    {
        qp::SpmcQueue<std::shared_ptr<Tracked>, 8, NumConsumers> queue;

        std::vector<int>         mismatch_at(NumConsumers, -1);
        std::vector<std::thread> consumers;
        for (std::size_t c = 0; c < NumConsumers; ++c) {
            consumers.emplace_back([&, c] {
                for (int expected = 0; expected < N; ++expected) {
                    std::optional<std::shared_ptr<Tracked>> v;
                    while (!(v = queue.try_pop(c))) std::this_thread::yield();
                    if ((*v)->v != expected) {
                        mismatch_at[c] = expected;
                        break;
                    }
                    // *v destroyed here -> refcount decrement, each iteration
                }
            });
        }

        std::thread producer([&] {
            for (int i = 0; i < N; ++i) {
                while (!queue.push(std::make_shared<Tracked>(i))) std::this_thread::yield();
            }
        });

        producer.join();
        for (auto& t : consumers) t.join();

        for (std::size_t c = 0; c < NumConsumers; ++c) EXPECT_EQ(mismatch_at[c], -1);
    }
    // Every Tracked must be destroyed exactly once.
    EXPECT_EQ(g_alive.load(), 0);
}
