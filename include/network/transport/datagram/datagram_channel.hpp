#pragma once
#include <concepts>
#include <cstdint>
#include <string_view>

#include "lent_slot_pool.hpp"
#include "rx_times.hpp"

namespace qp::network::transport::datagram {

/// Whether a channel can still carry datagrams.
enum class ChannelStatus : std::uint8_t { Open, Closed, Error };

/// One whole datagram lent by a channel, with when it arrived. Destroying it hands its slot back.
struct [[nodiscard]] Datagram {
    LentSlot      lent;  ///< Holds no slot when nothing was ready.
    RxTimes       rx_ts{};
    ChannelStatus status = ChannelStatus::Open;
};

/// Whole datagrams arriving at a local IPv4 address and port. read() lends one without
/// blocking, in the order the channel received them. A channel counts the datagrams it lost.
template <class C>
concept DatagramChannel = requires(C c, std::string_view ip, std::uint16_t port) {
    { c.bind(ip, port) } -> std::same_as<ChannelStatus>;  // Open once listening
    { c.read() } -> std::same_as<Datagram>;
    { c.dropped() } -> std::same_as<std::uint64_t>;    // arrived with no free slot
    { c.truncated() } -> std::same_as<std::uint64_t>;  // larger than a slot, thrown away
    { c.close() } -> std::same_as<void>;  // drops unread datagrams, lent ones stay valid
};

}  // namespace qp::network::transport::datagram
