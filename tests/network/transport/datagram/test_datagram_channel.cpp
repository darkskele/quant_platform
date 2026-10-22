#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

#include "datagram_channel.hpp"

using namespace qp::network::transport::datagram;

namespace {

// The least a type needs to be a channel.
struct NullChannel {
    ChannelStatus bind(std::string_view, std::uint16_t) { return ChannelStatus::Open; }

    Datagram read() { return {}; }

    std::uint64_t dropped() { return 0; }

    std::uint64_t truncated() { return 0; }

    void close() {}
};

static_assert(DatagramChannel<NullChannel>);
static_assert(!DatagramChannel<int>);

}  // namespace

TEST(Datagram, DefaultHoldsNoSlotAndIsOpen) {
    const Datagram d;
    EXPECT_FALSE(d.lent);
    EXPECT_EQ(d.rx_ts.user, 0);
    EXPECT_EQ(d.status, ChannelStatus::Open);
}
