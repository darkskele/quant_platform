#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "lent_mirror_ring.hpp"

using Ring = qp::LentMirrorRing<4096, 4>;
using qp::LentBytes;

namespace {

constexpr std::size_t kPage = 4096;

// Writes n bytes continuing the pattern from position pos.
template <class R>
void fill(R& ring, std::size_t n, std::uint64_t& pos) {
    const auto wrote = ring.write([&](std::span<std::byte> space) {
        n = std::min(n, space.size());
        for (std::size_t i = 0; i < n; ++i) space[i] = std::byte(pos++ % 251);
        return n;
    });
    ASSERT_EQ(wrote, n);
}

// Free space, measured by a write that writes nothing.
template <class R>
std::size_t space(R& ring) {
    std::size_t size = 0;
    ring.write([&](std::span<std::byte> s) {
        size = s.size();
        return std::size_t{0};
    });
    return size;
}

}  // namespace

TEST(LentMirrorRing, EmptyRingLendsNothing) {
    Ring ring;
    EXPECT_EQ(space(ring), kPage);
    const LentBytes l = ring.lend(10);
    EXPECT_TRUE(l.bytes().empty());
}

TEST(LentMirrorRing, LendsUpToWhatHasArrived) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, 10, pos);

    const LentBytes a = ring.lend(4);
    ASSERT_EQ(a.bytes().size(), 4u);
    EXPECT_EQ(a.bytes()[0], std::byte(0));
    const LentBytes b = ring.lend(100);
    ASSERT_EQ(b.bytes().size(), 6u);
    EXPECT_EQ(b.bytes()[0], std::byte(4));
    EXPECT_EQ(ring.unlent(), 0u);
}

TEST(LentMirrorRing, SpanAcrossTheEndIsContiguous) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, 3000, pos);
    (void)ring.lend(3000);

    // Starts at 3000 and wraps, so its second half is the ring's start seen through the mirror.
    fill(ring, 3000, pos);
    const LentBytes l = ring.lend(3000);
    ASSERT_EQ(l.bytes().size(), 3000u);
    for (std::size_t i = 0; i < l.bytes().size(); ++i)
        ASSERT_EQ(l.bytes()[i], std::byte((3000 + i) % 251));
}

TEST(LentMirrorRing, WritesThroughTheMirrorLandAtTheStart) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, kPage - 2, pos);
    (void)ring.lend(kPage);

    // Two bytes before the end, two through the mirror.
    fill(ring, 4, pos);
    (void)ring.lend(2);

    // Starts at offset zero, so it reads the first view directly.
    const LentBytes l = ring.lend(2);
    ASSERT_EQ(l.bytes().size(), 2u);
    EXPECT_EQ(l.bytes()[0], std::byte(kPage % 251));
    EXPECT_EQ(l.bytes()[1], std::byte((kPage + 1) % 251));
}

TEST(LentMirrorRing, DroppingLentBytesFreesTheirSpace) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, kPage, pos);
    {
        const LentBytes l = ring.lend(kPage);
        EXPECT_EQ(space(ring), 0u);
    }
    EXPECT_EQ(space(ring), kPage);
}

TEST(LentMirrorRing, OutOfOrderReleaseFreesOnlyBehindTheOldest) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, kPage, pos);
    LentBytes a = ring.lend(1000);
    LentBytes b = ring.lend(1000);
    EXPECT_EQ(space(ring), 0u);

    b = {};
    EXPECT_EQ(space(ring), 0u);
    a = {};
    EXPECT_EQ(space(ring), 2000u);
}

TEST(LentMirrorRing, StopsLendingAtMaxLent) {
    qp::LentMirrorRing<4096, 2> ring;
    std::uint64_t               pos = 0;
    fill(ring, 30, pos);
    LentBytes a = ring.lend(10);
    LentBytes b = ring.lend(10);
    {
        const LentBytes none = ring.lend(10);
        EXPECT_TRUE(none.bytes().empty());
    }
    a                 = {};
    const LentBytes c = ring.lend(10);
    EXPECT_EQ(c.bytes().size(), 10u);
}

TEST(LentMirrorRing, MovedLentBytesReleasesOnce) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, kPage, pos);
    LentBytes a = ring.lend(kPage);
    {
        LentBytes moved = std::move(a);
        EXPECT_EQ(space(ring), 0u);
    }
    EXPECT_EQ(space(ring), kPage);
}

TEST(LentMirrorRing, MovedRingKeepsLentBytesValid) {
    Ring          ring;
    std::uint64_t pos = 0;
    fill(ring, kPage, pos);
    LentBytes l     = ring.lend(kPage);
    Ring      moved = std::move(ring);
    EXPECT_EQ(space(moved), 0u);
    l = {};
    EXPECT_EQ(space(moved), kPage);
}

TEST(LentMirrorRing, RandomTrafficKeepsEveryByteInOrder) {
    qp::LentMirrorRing<4096, 8> ring;
    std::mt19937                rng(7);
    std::uint64_t               written = 0, checked = 0;
    std::vector<LentBytes>      held;

    for (int step = 0; step < 200'000; ++step) {
        const auto w = space(ring);
        if (w > 0) fill(ring, std::uniform_int_distribution<std::size_t>(0, w)(rng), written);

        LentBytes l = ring.lend(std::uniform_int_distribution<std::size_t>(1, 700)(rng));
        for (std::size_t i = 0; i < l.bytes().size(); ++i)
            ASSERT_EQ(l.bytes()[i], std::byte((checked + i) % 251));
        checked += l.bytes().size();
        if (!l.bytes().empty()) held.push_back(std::move(l));

        // Drop a random held range, so releases come out of order.
        if (!held.empty() && rng() % 2) {
            const auto k = rng() % held.size();
            held[k]      = std::move(held.back());
            held.pop_back();
        }
    }
    held.clear();
    EXPECT_GT(checked, 10 * kPage);
}
