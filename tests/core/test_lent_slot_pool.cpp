#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "lent_slot_pool.hpp"

using Pool = qp::LentSlotPool<2048, 8>;
using qp::LentSlot;

namespace {

// Fills a slot with bytes derived from tag.
void stamp(std::span<std::byte> slot, std::uint32_t tag) {
    for (std::size_t i = 0; i < slot.size(); ++i) slot[i] = std::byte((tag * 31 + i) % 251);
}

bool stamped(std::span<const std::byte> bytes, std::uint32_t tag, std::size_t offset) {
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (bytes[i] != std::byte((tag * 31 + offset + i) % 251)) return false;
    return true;
}

}  // namespace

TEST(LentSlotPool, StartsWithEverySlotFree) {
    Pool pool;
    EXPECT_EQ(pool.free(), 8u);
}

TEST(LentSlotPool, SlotsAreContiguousAndEqual) {
    Pool pool;
    for (std::uint32_t i = 0; i < 8; ++i) {
        ASSERT_EQ(pool.slot(i).size(), 2048u);
        EXPECT_EQ(pool.slot(i).data(), pool.slot(0).data() + i * 2048);
    }
}

TEST(LentSlotPool, AcquireGivesEachSlotOnceThenNothing) {
    Pool                    pool;
    std::set<std::uint32_t> seen;
    for (int i = 0; i < 8; ++i) {
        const auto s = pool.acquire();
        ASSERT_TRUE(s);
        EXPECT_TRUE(seen.insert(*s).second);
    }
    EXPECT_EQ(pool.free(), 0u);
    EXPECT_FALSE(pool.acquire());
    for (auto s : seen) pool.release(s);
}

TEST(LentSlotPool, LendViewsTheRangeAsked) {
    Pool       pool;
    const auto s = *pool.acquire();
    stamp(pool.slot(s), s);

    const LentSlot l = pool.lend(s, 42, 100);
    ASSERT_EQ(l.bytes().size(), 100u);
    EXPECT_EQ(l.bytes().data(), pool.slot(s).data() + 42);
    EXPECT_TRUE(stamped(l.bytes(), s, 42));
}

TEST(LentSlotPool, EmptyLendStillHoldsTheSlot) {
    Pool pool;
    {
        const LentSlot l = pool.lend(*pool.acquire(), 0, 0);
        EXPECT_TRUE(l.bytes().empty());
        EXPECT_EQ(pool.free(), 7u);
    }
    EXPECT_EQ(pool.free(), 8u);
}

TEST(LentSlotPool, DroppingLentSlotFreesIt) {
    Pool pool;
    {
        const LentSlot l = pool.lend(*pool.acquire(), 0, 2048);
        EXPECT_EQ(pool.free(), 7u);
    }
    EXPECT_EQ(pool.free(), 8u);
}

TEST(LentSlotPool, ReleaseFreesAnUnlentSlot) {
    Pool pool;
    pool.release(*pool.acquire());
    EXPECT_EQ(pool.free(), 8u);
}

TEST(LentSlotPool, LastFreedIsFirstTaken) {
    Pool       pool;
    const auto a = *pool.acquire();
    const auto b = *pool.acquire();
    pool.release(a);
    pool.release(b);
    EXPECT_EQ(*pool.acquire(), b);
    EXPECT_EQ(*pool.acquire(), a);
    pool.release(a);
    pool.release(b);
}

TEST(LentSlotPool, AnyOrderOfDropsFreesEverySlot) {
    Pool                  pool;
    std::vector<LentSlot> held;
    while (const auto s = pool.acquire()) held.push_back(pool.lend(*s, 0, 1));
    ASSERT_EQ(held.size(), 8u);

    std::shuffle(held.begin(), held.end(), std::mt19937(3));
    for (std::size_t i = 0; i < held.size(); ++i) {
        held[i] = {};
        EXPECT_EQ(pool.free(), i + 1);
    }

    std::set<std::uint32_t> seen;
    while (const auto s = pool.acquire()) seen.insert(*s);
    EXPECT_EQ(seen.size(), 8u);
    for (auto s : seen) pool.release(s);
}

TEST(LentSlotPool, MovedLentSlotReleasesOnce) {
    Pool     pool;
    LentSlot a = pool.lend(*pool.acquire(), 0, 1);
    {
        LentSlot moved = std::move(a);
        EXPECT_EQ(pool.free(), 7u);
    }
    EXPECT_EQ(pool.free(), 8u);
    a = {};
    EXPECT_EQ(pool.free(), 8u);
}

TEST(LentSlotPool, AssigningOverALentSlotFreesTheOldOne) {
    Pool     pool;
    LentSlot a = pool.lend(*pool.acquire(), 0, 1);
    a          = pool.lend(*pool.acquire(), 0, 1);
    EXPECT_EQ(pool.free(), 7u);
}

TEST(LentSlotPool, MovedPoolKeepsLentSlotsValid) {
    Pool       pool;
    const auto s = *pool.acquire();
    stamp(pool.slot(s), s);
    LentSlot l     = pool.lend(s, 0, 2048);
    Pool     moved = std::move(pool);
    EXPECT_EQ(moved.free(), 7u);
    EXPECT_TRUE(stamped(l.bytes(), s, 0));
    l = {};
    EXPECT_EQ(moved.free(), 8u);
}

TEST(LentSlotPool, RandomTrafficNeverSharesASlot) {
    qp::LentSlotPool<2048, 64>                      pool;
    std::mt19937                                    rng(7);
    std::vector<std::pair<LentSlot, std::uint32_t>> held;
    std::uint32_t                                   tag = 0;

    for (int step = 0; step < 200'000; ++step) {
        if (const auto s = pool.acquire()) {
            stamp(pool.slot(*s), ++tag);
            const auto off = std::uniform_int_distribution<std::size_t>(0, 64)(rng);
            const auto len = std::uniform_int_distribution<std::size_t>(0, 2048 - off)(rng);
            held.emplace_back(pool.lend(*s, off, len), tag);
        }
        // Drop a random held slot, checking nothing overwrote it while it was out.
        if (!held.empty() && rng() % 2) {
            const auto k              = rng() % held.size();
            const auto& [slot, owner] = held[k];
            const auto off =
                static_cast<std::size_t>(slot.bytes().data() - pool.slot(0).data()) % 2048;
            ASSERT_TRUE(stamped(slot.bytes(), owner, off));
            held[k] = std::move(held.back());
            held.pop_back();
        }
        ASSERT_EQ(pool.free() + held.size(), 64u);
    }
    held.clear();
    EXPECT_EQ(pool.free(), 64u);
}
