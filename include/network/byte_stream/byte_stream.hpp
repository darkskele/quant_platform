#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "lent_mirror_ring.hpp"

namespace qp::network::byte_stream {

/// Nanoseconds on the monotonic clock.
using RxTime = std::int64_t;

/// Whether a stream can still carry bytes. Full means a write found no room, and the stream
/// stays open.
enum class StreamStatus : std::uint8_t { Open, Closed, Error, Full };

/// Bytes lent by a stream, with when they arrived. Destroying it hands them back.
struct [[nodiscard]] Readable {
    LentBytes    lent;
    RxTime       rx_ts  = 0;  ///< When the newest bytes became visible.
    StreamStatus status = StreamStatus::Open;
};

/// What a write accepted. The stream delivers every accepted byte.
struct Written {
    std::size_t  bytes  = 0;
    StreamStatus status = StreamStatus::Open;
};

/// What a flush sent and what is still waiting to go.
struct Flushed {
    std::size_t  sent    = 0;
    std::size_t  pending = 0;
    StreamStatus status  = StreamStatus::Open;
};

/// An ordered byte stream to a remote IPv4 address and port. read(n) lends at most n bytes
/// without blocking, fewer when fewer have arrived. write(max, fill) lends up to max bytes of
/// space for fill to write in place and sends them. A stream can close and connect again.
template <class S>
concept ByteStream = requires(S s, std::string_view ip, std::uint16_t port, std::size_t n) {
    { s.connect(ip, port) } -> std::same_as<StreamStatus>;  // blocks, Open once connected
    { s.read(n) } -> std::same_as<Readable>;
    {
        s.write(n, [](std::span<std::byte>) noexcept { return std::size_t{0}; })
    } -> std::same_as<Written>;
    { s.flush() } -> std::same_as<Flushed>;  // sends bytes a write left behind
    { s.close() } -> std::same_as<void>;     // drops unread bytes, lent ones stay valid
};

}  // namespace qp::network::byte_stream
