#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "message.hpp"
#include "stream_framer.hpp"

using namespace qp::network::rig;

namespace {

// count messages of varied sizes back to back, seq from 0.
std::vector<std::byte> stream_of(std::uint64_t count) {
    std::vector<std::byte> out;
    for (std::uint64_t seq = 0; seq < count; ++seq) {
        const auto size = static_cast<std::uint32_t>(sizeof(MessageHeader) + seq % 1500);
        const auto at   = out.size();
        out.resize(at + size, std::byte{0xab});
        write_header(std::span(out).subspan(at), {size, 0, seq, static_cast<std::int64_t>(seq)});
    }
    return out;
}

}  // namespace

TEST(StreamFramer, WholeMessagesInOneFeed) {
    const auto                 bytes = stream_of(10);
    StreamFramer               f;
    std::vector<std::uint64_t> seqs;
    const auto n = f.feed(bytes, [&](const MessageHeader& h) { seqs.push_back(h.seq); });
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, 10u);
    for (std::uint64_t i = 0; i < 10; ++i) EXPECT_EQ(seqs[i], i);
}

TEST(StreamFramer, MessagesSplitAnywhereComeOutWholeAndInOrder) {
    const auto    bytes = stream_of(2000);
    StreamFramer  f;
    std::mt19937  rng(9);
    std::uint64_t next = 0;
    std::size_t   at   = 0;
    while (at < bytes.size()) {
        // Cuts land inside headers, inside payloads and on boundaries.
        const auto len = std::min<std::size_t>(rng() % 3000 + 1, bytes.size() - at);
        const auto n   = f.feed(std::span(bytes).subspan(at, len), [&](const MessageHeader& h) {
            ASSERT_EQ(h.seq, next);
            ASSERT_EQ(h.send_ts, static_cast<std::int64_t>(next));
            ASSERT_EQ(h.size, sizeof(MessageHeader) + next % 1500);
            ++next;
        });
        ASSERT_TRUE(n);
        at += len;
    }
    EXPECT_EQ(next, 2000u);
}

TEST(StreamFramer, OneByteAtATime) {
    const auto    bytes = stream_of(50);
    StreamFramer  f;
    std::uint64_t next = 0;
    for (const std::byte b : bytes)
        ASSERT_TRUE(
            f.feed(std::span(&b, 1), [&](const MessageHeader& h) { EXPECT_EQ(h.seq, next++); }));
    EXPECT_EQ(next, 50u);
}

TEST(StreamFramer, ImpossibleSizeIsBadFraming) {
    std::vector<std::byte> bytes(sizeof(MessageHeader));
    write_header(bytes, {4, 0, 0, 0});
    StreamFramer f;
    EXPECT_FALSE(f.feed(bytes, [](const MessageHeader&) {}));
}
