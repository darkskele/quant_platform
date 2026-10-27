#pragma once
#include <time.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace qp::network::rig {

/// Leads every rig message. size is the whole message, header included, so on a byte stream it
/// also says where the next one starts.
struct MessageHeader {
    std::uint32_t size    = 0;
    std::uint32_t flags   = 0;
    std::uint64_t seq     = 0;
    std::int64_t  send_ts = 0;  ///< The peer's monotonic clock just before it sent.
};

static_assert(sizeof(MessageHeader) == 24);

/// MessageHeader::flags, low bits. The top 16 bits carry the sending peer's id.
inline constexpr std::uint32_t WARMUP = 1;  ///< Not recorded.
inline constexpr std::uint32_t ACK    = 2;  ///< The receiver acks it, so the peer sends the next.
inline constexpr std::uint32_t END    = 4;  ///< This peer's run is over.

/// The most peers one receiver tells apart.
inline constexpr std::uint32_t MAX_PEERS = 64;

inline constexpr std::uint32_t with_sender(std::uint32_t flags, std::uint32_t sender) noexcept {
    return flags | sender << 16;
}

inline constexpr std::uint32_t sender_of(const MessageHeader& h) noexcept { return h.flags >> 16; }

/// The ack carrying this seq says the receiver is ready.
inline constexpr std::uint64_t READY = ~std::uint64_t{0};

/// The largest message the peer sends.
inline constexpr std::size_t MAX_MESSAGE = 65'507;

inline MessageHeader read_header(std::span<const std::byte> bytes) noexcept {
    MessageHeader h;
    std::memcpy(&h, bytes.data(), sizeof h);
    return h;
}

inline void write_header(std::span<std::byte> bytes, const MessageHeader& h) noexcept {
    std::memcpy(bytes.data(), &h, sizeof h);
}

inline std::int64_t mono_ns() noexcept {
    timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

}  // namespace qp::network::rig
