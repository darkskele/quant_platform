#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "byte_stream.hpp"
#include "lent_mirror_ring.hpp"

namespace qp::network::byte_stream {

/// A ByteStream over a kernel TCP socket. Reads go straight from the socket into a ring the
/// stream lends from.
/// @tparam RingSize ring bytes, a power of two and a page multiple.
/// @tparam MaxLent Readables alive at once, a power of two.
template <std::size_t RingSize, std::size_t MaxLent>
class KernelTcpStream {
   public:
    /// Throws if the ring cannot be mapped.
    KernelTcpStream() = default;

    ~KernelTcpStream() {
        if (fd_ >= 0) ::close(fd_);
    }

    KernelTcpStream(KernelTcpStream&& other) noexcept
        : ring_(std::move(other.ring_)),
          fd_(std::exchange(other.fd_, -1)),
          rx_ts_(other.rx_ts_),
          status_(other.status_) {}

    KernelTcpStream& operator=(KernelTcpStream&&)      = delete;
    KernelTcpStream(const KernelTcpStream&)            = delete;
    KernelTcpStream& operator=(const KernelTcpStream&) = delete;

    /// Closes any current connection first. Error on a bad address or a failed connect.
    StreamStatus connect(std::string_view ip, std::uint16_t port) noexcept {
        close();
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        char text[INET_ADDRSTRLEN]{};
        if (ip.size() >= sizeof text) return fail();
        std::ranges::copy(ip, text);
        if (::inet_pton(AF_INET, text, &addr.sin_addr) != 1) return fail();

        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd_ < 0) return fail();
        if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0)
            return fail();
        const int flags = ::fcntl(fd_, F_GETFL);
        if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != 0) return fail();
        return status_ = StreamStatus::Open;
    }

    Readable read(std::size_t n) noexcept {
        // Only an empty ring receives, so every lend comes from one recv and one rx_ts.
        if (ring_.unlent() == 0 && status_ == StreamStatus::Open) receive();
        return {ring_.lend(n), rx_ts_, status_};
    }

    void close() noexcept {
        if (fd_ >= 0) ::close(std::exchange(fd_, -1));
        ring_.drop_unlent();
        rx_ts_  = 0;
        status_ = StreamStatus::Closed;
    }

   private:
    StreamStatus fail() noexcept { return status_ = StreamStatus::Error; }

    void receive() noexcept {
        ssize_t got = -1;
        int     err = EAGAIN;
        ring_.write([&](std::span<std::byte> space) noexcept {
            // A zero length recv returns 0, which reads as a close.
            if (space.empty()) return std::size_t{0};
            got = ::recv(fd_, space.data(), space.size(), 0);
            if (got > 0) {
                rx_ts_ = now();
                return static_cast<std::size_t>(got);
            }
            err = errno;
            return std::size_t{0};
        });
        if (got == 0)
            status_ = StreamStatus::Closed;
        else if (got < 0 && err != EAGAIN && err != EWOULDBLOCK && err != EINTR)
            status_ = StreamStatus::Error;
    }

    static RxTime now() noexcept {
        timespec ts{};
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<RxTime>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
    }

    LentMirrorRing<RingSize, MaxLent> ring_;
    int                               fd_     = -1;
    RxTime                            rx_ts_  = 0;
    StreamStatus                      status_ = StreamStatus::Closed;  // Open once connected
};

}  // namespace qp::network::byte_stream
