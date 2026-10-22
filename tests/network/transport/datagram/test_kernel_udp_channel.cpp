#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "datagram_channel.hpp"
#include "kernel_udp_channel.hpp"

using namespace qp::network::transport;
using namespace qp::network::transport::datagram;
using Channel = KernelUdpChannel<2048, 16, 4>;

static_assert(DatagramChannel<Channel>);
static_assert(
    DatagramChannel<KernelUdpChannel<2048, 16, 4, RxStamps{.kernel = true, .nic = true}>>);

namespace {

constexpr auto kLocal = "127.0.0.1";

// A UDP socket sending to one loopback port.
class Sender {
   public:
    explicit Sender(std::uint16_t port) : fd_(::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) {
        to_.sin_family      = AF_INET;
        to_.sin_port        = htons(port);
        to_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    ~Sender() { ::close(fd_); }

    void send(std::span<const std::byte> bytes) const {
        ASSERT_EQ(::sendto(fd_, bytes.data(), bytes.size(), 0,
                           reinterpret_cast<const sockaddr*>(&to_), sizeof to_),
                  static_cast<ssize_t>(bytes.size()));
    }

   private:
    int         fd_;
    sockaddr_in to_{};
};

// n bytes, the first four the sequence number, the rest derived from it.
std::vector<std::byte> payload(std::uint32_t seq, std::size_t n) {
    std::vector<std::byte> out(n);
    for (std::size_t i = 0; i < n; ++i) out[i] = std::byte((seq * 7 + i) % 251);
    if (n >= sizeof seq) std::memcpy(out.data(), &seq, sizeof seq);
    return out;
}

bool matches(std::span<const std::byte> bytes, std::uint32_t seq, std::size_t n) {
    const auto want = payload(seq, n);
    return bytes.size() == n && std::memcmp(bytes.data(), want.data(), n) == 0;
}

// Reads until one datagram arrives or a second passes.
template <class C>
Datagram read_one(C& c) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    for (;;) {
        Datagram d = c.read();
        if (d.lent || std::chrono::steady_clock::now() > deadline) return d;
    }
}

}  // namespace

TEST(KernelUdpChannel, BadAddressIsError) {
    Channel c;
    EXPECT_EQ(c.bind("not an ip", 0), ChannelStatus::Error);
    EXPECT_EQ(c.read().status, ChannelStatus::Error);
}

TEST(KernelUdpChannel, BindsAFreePort) {
    Channel c;
    EXPECT_EQ(c.local_port(), 0);
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    EXPECT_NE(c.local_port(), 0);
}

TEST(KernelUdpChannel, ReadsNothingBeforeData) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    const Datagram d = c.read();
    EXPECT_FALSE(d.lent);
    EXPECT_EQ(d.status, ChannelStatus::Open);
}

TEST(KernelUdpChannel, ReadsWholeDatagramsInOrder) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender            s(c.local_port());
    const std::size_t sizes[] = {1, 100, 1472, 2048, 7};
    for (std::uint32_t i = 0; i < 5; ++i) s.send(payload(i, sizes[i]));

    for (std::uint32_t i = 0; i < 5; ++i) {
        const Datagram d = read_one(c);
        ASSERT_TRUE(d.lent);
        EXPECT_TRUE(matches(d.lent.bytes(), i, sizes[i])) << "datagram " << i;
        EXPECT_GT(d.rx_ts.user, 0);
        EXPECT_EQ(d.rx_ts.kernel, 0);
    }
    EXPECT_FALSE(c.read().lent);
}

TEST(KernelUdpChannel, ZeroLengthDatagramHoldsASlot) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender(c.local_port()).send({});
    const Datagram d = read_one(c);
    EXPECT_TRUE(d.lent);
    EXPECT_TRUE(d.lent.bytes().empty());
}

TEST(KernelUdpChannel, OversizedDatagramIsCountedNotLent) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender s(c.local_port());
    s.send(payload(0, 2049));
    s.send(payload(1, 10));

    const Datagram d = read_one(c);
    ASSERT_TRUE(d.lent);
    EXPECT_TRUE(matches(d.lent.bytes(), 1, 10));
    EXPECT_EQ(c.truncated(), 1u);
}

TEST(KernelUdpChannel, LentDatagramsHandBackInAnyOrder) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender       s(c.local_port());
    std::mt19937 rng(5);

    // Holds up to 12 of the 16 slots at once, dropping a random one each round.
    std::vector<std::pair<Datagram, std::uint32_t>> held;
    for (std::uint32_t seq = 0; seq < 5000; ++seq) {
        s.send(payload(seq, 64 + seq % 1400));
        Datagram d = read_one(c);
        ASSERT_TRUE(d.lent) << "datagram " << seq;
        ASSERT_TRUE(matches(d.lent.bytes(), seq, 64 + seq % 1400)) << "datagram " << seq;
        held.emplace_back(std::move(d), seq);
        if (held.size() == 12) {
            const auto k = rng() % held.size();
            ASSERT_TRUE(
                matches(held[k].first.lent.bytes(), held[k].second, 64 + held[k].second % 1400));
            held[k] = std::move(held.back());
            held.pop_back();
        }
    }
    EXPECT_EQ(c.dropped(), 0u);
    EXPECT_EQ(c.truncated(), 0u);
}

TEST(KernelUdpChannel, DropsWithNoFreeSlotAreCounted) {
    KernelUdpChannel<2048, 4, 4> c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender s(c.local_port());

    // Every slot lent, so the flood can only queue in the socket until it overflows.
    for (std::uint32_t i = 0; i < 4; ++i) s.send(payload(i, 1000));
    std::vector<Datagram> held;
    for (int i = 0; i < 4; ++i) held.push_back(read_one(c));
    ASSERT_FALSE(c.read().lent);
    for (std::uint32_t i = 0; i < 20'000; ++i) s.send(payload(i, 1000));

    // The kernel reports the count on the first datagram queued after the drops.
    held.clear();
    while (read_one(c).lent) {
    }
    s.send(payload(0, 10));
    ASSERT_TRUE(read_one(c).lent);
    EXPECT_GT(c.dropped(), 0u);
}

TEST(KernelUdpChannel, KernelStampComesShortlyBeforeUser) {
    KernelUdpChannel<2048, 16, 4, RxStamps{.kernel = true}> c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender(c.local_port()).send(payload(0, 100));

    const Datagram d = read_one(c);
    ASSERT_TRUE(d.lent);
    EXPECT_GT(d.rx_ts.kernel, 0);
    EXPECT_LE(d.rx_ts.kernel, d.rx_ts.user);
    EXPECT_LT(d.rx_ts.user - d.rx_ts.kernel, 1'000'000'000);
    EXPECT_EQ(d.rx_ts.nic, 0);
}

// Loopback has no NIC clock, so asking for one binds and stamps nothing.
TEST(KernelUdpChannel, NicStampIsZeroWithoutHardware) {
    KernelUdpChannel<2048, 16, 4, RxStamps{.nic = true}> c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender(c.local_port()).send(payload(0, 100));

    const Datagram d = read_one(c);
    ASSERT_TRUE(d.lent);
    EXPECT_EQ(d.rx_ts.nic, 0);
    EXPECT_EQ(d.rx_ts.kernel, 0);
}

TEST(KernelUdpChannel, CloseDropsUnreadAndKeepsLent) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender s(c.local_port());
    for (std::uint32_t i = 0; i < 3; ++i) s.send(payload(i, 100));
    const Datagram kept = read_one(c);
    ASSERT_TRUE(kept.lent);

    c.close();
    const Datagram after = c.read();
    EXPECT_FALSE(after.lent);
    EXPECT_EQ(after.status, ChannelStatus::Closed);
    EXPECT_TRUE(matches(kept.lent.bytes(), 0, 100));
}

TEST(KernelUdpChannel, BindsAgainAfterClose) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender(c.local_port()).send(payload(0, 100));
    c.close();

    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    EXPECT_FALSE(c.read().lent);
    Sender(c.local_port()).send(payload(1, 50));
    const Datagram d = read_one(c);
    ASSERT_TRUE(d.lent);
    EXPECT_TRUE(matches(d.lent.bytes(), 1, 50));
}

TEST(KernelUdpChannel, MovedChannelKeepsReading) {
    Channel c;
    ASSERT_EQ(c.bind(kLocal, 0), ChannelStatus::Open);
    Sender s(c.local_port());
    s.send(payload(0, 100));
    s.send(payload(1, 100));
    Datagram first = read_one(c);
    ASSERT_TRUE(first.lent);

    Channel        moved  = std::move(c);
    const Datagram second = read_one(moved);
    ASSERT_TRUE(second.lent);
    EXPECT_TRUE(matches(first.lent.bytes(), 0, 100));
    EXPECT_TRUE(matches(second.lent.bytes(), 1, 100));
    // Handed back before the channel holding its slot goes.
    first.lent = {};
}
