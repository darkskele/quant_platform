#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "spmc_ring.hpp"

TEST(SpmcRing, PushTryPopFifoWithinCapacityNoWait) {
    qp::SpmcRing<int, 4, 1> ring;  // 3 pushes < Capacity -> no gating wait triggered
    ring.push(1);
    ring.push(2);
    ring.push(3);

    EXPECT_EQ(*ring.try_pop(0), 1);
    EXPECT_EQ(*ring.try_pop(0), 2);
    EXPECT_EQ(*ring.try_pop(0), 3);
    EXPECT_FALSE(ring.try_pop(0).has_value());  // caught up

    EXPECT_EQ(ring.full_count(), 0u);  // never wrapped -> push() never had to wait
}

TEST(SpmcRing, EachConsumerSeesTheFullStreamIndependently) {
    qp::SpmcRing<int, 8, 3> ring;
    for (int i = 0; i < 5; ++i) ring.push(i);  // < Capacity, single-threaded-safe

    for (int i = 0; i < 5; ++i) EXPECT_EQ(*ring.try_pop(0), i);
    EXPECT_FALSE(ring.try_pop(0).has_value());

    // Consumers 1 and 2 haven't read anything yet — still see the full
    // stream from the start, independent of consumer 0's progress.
    for (std::size_t c : {std::size_t{1}, std::size_t{2}}) {
        for (int i = 0; i < 5; ++i) EXPECT_EQ(*ring.try_pop(c), i);
        EXPECT_FALSE(ring.try_pop(c).has_value());
    }
}

namespace {
std::atomic<int> g_alive{0};

// Copy-constructible (unlike SpscQueue's test Tracked): SpmcRing::try_pop
// copies out of the slot rather than moving, since other consumers may
// still need it.
struct Tracked {
    int v;

    explicit Tracked(int x) : v(x) { ++g_alive; }

    Tracked(const Tracked& o) : v(o.v) { ++g_alive; }

    Tracked(Tracked&& o) noexcept : v(o.v) { ++g_alive; }

    // Assignment overwrites an existing live value in place -- no
    // construction/destruction, so g_alive is untouched. Needed for
    // `v = ring.try_pop(...)` in the test loops below (optional<Tracked>'s
    // move-assignment requires it).
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

TEST(SpmcRing, DestructorCleansUpAllLiveSlotsNoWraparound) {
    g_alive = 0;
    {
        qp::SpmcRing<Tracked, 8, 1> ring;
        for (int i = 0; i < 8; ++i) ring.push(i);  // fills exactly to Capacity, no wait
        EXPECT_EQ(g_alive.load(), 8);
    }  // destructor must destroy all 8 still-constructed slots
    EXPECT_EQ(g_alive.load(), 0);
}

TEST(SpmcRing, WraparoundLeavesExactlyCapacityLiveAfterConsumerCatchesUp) {
    g_alive         = 0;
    constexpr int N = 50;
    {
        qp::SpmcRing<Tracked, 8, 1> ring;

        std::thread consumer([&] {
            for (int i = 0; i < N; ++i) {
                std::optional<Tracked> v;
                while (!(v = ring.try_pop(0))) std::this_thread::yield();
                // v destroyed here, each iteration — no lingering copies
            }
        });

        for (int i = 0; i < N; ++i) ring.push(i);
        consumer.join();

        // push() never destroys on read — only the most recent Capacity
        // pushes are ever constructed at once, regardless of consumption.
        EXPECT_EQ(g_alive.load(), 8);
    }
    EXPECT_EQ(g_alive.load(), 0);
}

TEST(SpmcRing, SlowConsumerNeverMissesAnEvent) {
    constexpr int           N = 40;
    qp::SpmcRing<int, 4, 1> ring;

    std::vector<int> received;
    std::thread      consumer([&] {
        // Force the producer past its spin/yield budget into the sleep-
        // backoff tier before this consumer reads anything.
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        for (int i = 0; i < N; ++i) {
            std::optional<int> v;
            while (!(v = ring.try_pop(0))) std::this_thread::yield();
            received.push_back(*v);
        }
    });

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) ring.push(i);
    auto elapsed = std::chrono::steady_clock::now() - start;

    consumer.join();

    // Producer had to actually wait for the deliberately-delayed consumer
    // rather than racing ahead and silently overwriting unread slots.
    EXPECT_GE(elapsed, std::chrono::milliseconds(15));
    EXPECT_GT(ring.full_count(), 0u);  // and the stall was actually counted

    ASSERT_EQ(received.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) EXPECT_EQ(received[i], i);
}

TEST(SpmcRing, ManyConcurrentConsumersPreserveOrderAndCompleteness) {
    constexpr int                            N            = 50'000;
    constexpr std::size_t                    NumConsumers = 6;
    qp::SpmcRing<int, 1 << 10, NumConsumers> ring;

    // GTest's ASSERT_*/EXPECT_* aren't meant to be evaluated off the main
    // test thread (same convention as SpscQueue's threaded test) — each
    // consumer records its first mismatch and the test thread asserts on
    // it after every thread has joined.
    std::vector<int>         mismatch_at(NumConsumers, -1);
    std::vector<int>         mismatch_got(NumConsumers, -1);
    std::vector<std::thread> consumers;
    for (std::size_t c = 0; c < NumConsumers; ++c) {
        consumers.emplace_back([&, c] {
            for (int expected = 0; expected < N; ++expected) {
                std::optional<int> v;
                while (!(v = ring.try_pop(c))) std::this_thread::yield();
                if (*v != expected) {
                    mismatch_at[c]  = expected;
                    mismatch_got[c] = *v;
                    break;
                }
            }
        });
    }

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) ring.push(i);
    });

    producer.join();
    for (auto& t : consumers) t.join();

    for (std::size_t c = 0; c < NumConsumers; ++c) {
        EXPECT_EQ(mismatch_at[c], -1)
            << "consumer " << c << " expected " << mismatch_at[c] << " got " << mismatch_got[c];
    }
}

TEST(SpmcRing, SharedPtrConsumersShareOwnershipNoDoubleFreeOrLeak) {
    g_alive                            = 0;
    constexpr int         N            = 200;
    constexpr std::size_t NumConsumers = 4;
    {
        qp::SpmcRing<std::shared_ptr<Tracked>, 8, NumConsumers> ring;

        std::vector<int>         mismatch_at(NumConsumers, -1);
        std::vector<std::thread> consumers;
        for (std::size_t c = 0; c < NumConsumers; ++c) {
            consumers.emplace_back([&, c] {
                for (int expected = 0; expected < N; ++expected) {
                    std::optional<std::shared_ptr<Tracked>> v;
                    while (!(v = ring.try_pop(c))) std::this_thread::yield();
                    if ((*v)->v != expected) {
                        mismatch_at[c] = expected;
                        break;
                    }
                    // *v destroyed here -> refcount decrement, each iteration
                }
            });
        }

        std::thread producer([&] {
            for (int i = 0; i < N; ++i) ring.push(std::make_shared<Tracked>(i));
        });

        producer.join();
        for (auto& t : consumers) t.join();

        for (std::size_t c = 0; c < NumConsumers; ++c) EXPECT_EQ(mismatch_at[c], -1);
    }
    // Every Tracked must be destroyed exactly once: a stuck positive count
    // would mean a leak, and a double-free would already have crashed the
    // process under TSan/ASan before reaching this assertion.
    EXPECT_EQ(g_alive.load(), 0);
}
