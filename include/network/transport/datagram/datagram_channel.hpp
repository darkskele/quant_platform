#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "lent_slot_pool.hpp"
#include "rx_times.hpp"

namespace qp::network::transport::datagram {

/// Whether a channel can still carry datagrams. Full means a write or a retarget found no room,
/// and the channel stays open.
enum class ChannelStatus : std::uint8_t { Open, Closed, Error, Full };

/// One whole datagram lent by a channel, with when it arrived. Destroying it hands its slot back.
struct [[nodiscard]] Datagram {
    LentSlot      lent;  ///< Holds no slot when nothing was ready.
    RxTimes       rx_ts{};
    ChannelStatus status = ChannelStatus::Open;
};

/// The datagram a write accepted, by its length. The channel sends every accepted datagram.
struct Written {
    std::size_t   bytes  = 0;
    ChannelStatus status = ChannelStatus::Open;
};

/// Datagrams a flush sent and datagrams still waiting to go.
struct Flushed {
    std::size_t   sent    = 0;
    std::size_t   pending = 0;
    ChannelStatus status  = ChannelStatus::Open;
};

/// Whole datagrams both ways over IPv4. read() lends one that arrived at the bound address,
/// without blocking, in the order the channel received them. write(max, fill) lends up to max
/// bytes of space for fill to write one datagram in place, and sends it to the target. A channel
/// counts the datagrams it lost.
template <class C>
concept DatagramChannel = requires(C c, std::string_view ip, std::uint16_t port, std::size_t n) {
    { c.bind(ip, port) } -> std::same_as<ChannelStatus>;    // Open once listening
    { c.target(ip, port) } -> std::same_as<ChannelStatus>;  // where writes go from now on
    { c.read() } -> std::same_as<Datagram>;
    {
        c.write(n, [](std::span<std::byte>) noexcept { return std::size_t{0}; })
    } -> std::same_as<Written>;
    { c.flush() } -> std::same_as<Flushed>;            // sends datagrams a write left waiting
    { c.dropped() } -> std::same_as<std::uint64_t>;    // arrived with no free slot
    { c.truncated() } -> std::same_as<std::uint64_t>;  // larger than a slot, thrown away
    { c.close() } -> std::same_as<void>;  // drops unread and unsent datagrams, lent ones stay valid
};

}  // namespace qp::network::transport::datagram
