#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <thread>
#include <vector>

#include "slot_ring.hpp"

TEST(SlotRing, PlaceAndTakeInPositionOrder) {
    qp::SlotRing<int, 4> ring;
    for (std::size_t i = 0; i < 4; ++i) EXPECT_TRUE(ring.place(i, static_cast<int>(i) + 1));

    for (std::size_t i = 0; i < 4; ++i) EXPECT_EQ(*ring.take(i), static_cast<int>(i) + 1);
    EXPECT_FALSE(ring.take(4).has_value());
}

// The reason the ring exists. Producers finish backwards and the consumer still
// sees plan order.
TEST(SlotRing, OutOfOrderPlacementReadsInOrder) {
    qp::SlotRing<int, 4> ring;
    EXPECT_TRUE(ring.place(3, 40));
    EXPECT_TRUE(ring.place(1, 20));

    EXPECT_FALSE(ring.take(0).has_value()) << "position 0 was never placed";

    EXPECT_TRUE(ring.place(0, 10));
    EXPECT_EQ(*ring.take(0), 10);
    EXPECT_EQ(*ring.take(1), 20);
    EXPECT_FALSE(ring.take(2).has_value());

    EXPECT_TRUE(ring.place(2, 30));
    EXPECT_EQ(*ring.take(2), 30);
    EXPECT_EQ(*ring.take(3), 40);
}

TEST(SlotRing, ReadyMatchesWhatTakeWouldGive) {
    qp::SlotRing<int, 4> ring;
    EXPECT_FALSE(ring.ready(0));
    EXPECT_TRUE(ring.place(0, 7));
    EXPECT_TRUE(ring.ready(0));
    EXPECT_EQ(*ring.take(0), 7);
    EXPECT_FALSE(ring.ready(0));
}

// A slot is held from the placement until the take, so the lap after it is
// refused until then.
TEST(SlotRing, PlacingALapEarlyIsRefused) {
    qp::SlotRing<int, 4> ring;
    EXPECT_TRUE(ring.place(0, 1));
    EXPECT_FALSE(ring.place(4, 2)) << "slot 0 still holds position 0";

    EXPECT_EQ(*ring.take(0), 1);
    EXPECT_TRUE(ring.place(4, 2));
    EXPECT_EQ(*ring.take(4), 2);
}

TEST(SlotRing, ManyLapsKeepOrder) {
    constexpr int        kLaps = 10'000;
    qp::SlotRing<int, 4> ring;
    for (int i = 0; i < kLaps; ++i) {
        ASSERT_TRUE(ring.place(static_cast<std::size_t>(i), i));
        ASSERT_EQ(*ring.take(static_cast<std::size_t>(i)), i);
    }
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

TEST(SlotRing, DestructorCleansUpWhatWasNeverTaken) {
    g_alive = 0;
    {
        qp::SlotRing<Tracked, 8> ring;
        for (std::size_t i = 0; i < 5; ++i) EXPECT_TRUE(ring.place(i, static_cast<int>(i)));
        EXPECT_EQ(g_alive, 5);
        for (std::size_t i = 0; i < 3; ++i) EXPECT_EQ(ring.take(i)->v, static_cast<int>(i));
        EXPECT_EQ(g_alive, 2);
    }  // the two placed but never taken must still be destroyed
    EXPECT_EQ(g_alive, 0);
}

// Placement is what happens on the fetch workers, one thread per position, and
// the reader is the engine thread walking positions in order.
TEST(SlotRing, ConcurrentProducersOneReaderLoseNothing) {
    constexpr std::size_t        kCapacity = 8;
    constexpr int                kTotal    = 20'000;
    qp::SlotRing<int, kCapacity> ring;

    std::atomic<std::size_t> handed{0};
    std::atomic<bool>        bad_place{false};
    std::atomic<std::size_t> read{0};

    // Producers claim a position each, then wait for its slot to free, which is
    // what the caller's own window would otherwise guarantee.
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&] {
            for (;;) {
                const auto position = handed.fetch_add(1, std::memory_order_relaxed);
                if (position >= kTotal) return;
                while (position >= read.load(std::memory_order_acquire) + kCapacity)
                    std::this_thread::yield();
                if (!ring.place(position, static_cast<int>(position)))
                    bad_place.store(true, std::memory_order_relaxed);
            }
        });
    }

    for (std::size_t position = 0; position < kTotal; ++position) {
        std::optional<int> value;
        while (!(value = ring.take(position))) std::this_thread::yield();
        ASSERT_EQ(*value, static_cast<int>(position));
        read.store(position + 1, std::memory_order_release);
    }

    for (auto& t : producers) t.join();
    EXPECT_FALSE(bad_place.load()) << "a slot was written before it was free";
}
