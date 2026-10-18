#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "lent_mirror_ring.hpp"

namespace qp::network::byte_stream {

/// Nanoseconds on the monotonic clock.
using RxTime = std::int64_t;

/// Whether a stream can still deliver bytes.
enum class StreamStatus : std::uint8_t { Open, Closed, Error };

/// Bytes lent by a stream, with when they arrived. Destroying it hands them back.
struct [[nodiscard]] Readable {
    LentBytes    lent;
    RxTime       rx_ts  = 0;  ///< When the newest bytes became visible.
    StreamStatus status = StreamStatus::Open;
};

/// An ordered byte stream to a remote IPv4 address and port. read(n) lends at most n bytes
/// without blocking, fewer when fewer have arrived.
template <class S>
concept ByteStream = requires(S s, std::string_view ip, std::uint16_t port, std::size_t n) {
    { s.connect(ip, port) } -> std::same_as<StreamStatus>;  // blocks, Open once connected
    { s.read(n) } -> std::same_as<Readable>;
};

}  // namespace qp::network::byte_stream
