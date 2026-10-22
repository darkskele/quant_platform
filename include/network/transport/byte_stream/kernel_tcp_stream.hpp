#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "byte_stream.hpp"
#include "lent_mirror_ring.hpp"

namespace qp::network::transport::byte_stream {

/// A ByteStream over a kernel TCP socket. Reads go straight from the socket into a ring the
/// stream lends from. Writes fill a second ring the stream sends from.
/// @tparam RxSize receive ring bytes, a power of two and a page multiple.
/// @tparam MaxLent Readables alive at once, a power of two.
/// @tparam TxSize send ring bytes, a power of two and a page multiple. The most one write can
/// take.
/// @tparam Stamps receive stamps taken besides the user one.
template <std::size_t RxSize, std::size_t MaxLent, std::size_t TxSize, RxStamps Stamps = RxStamps{}>
class KernelTcpStream {
    static constexpr bool STAMPED = Stamps.kernel || Stamps.nic;
    static constexpr int  STAMP_FLAGS =
        (Stamps.kernel ? SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE : 0) |
        (Stamps.nic ? SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE : 0);
    // Room for the stamps one recvmsg returns.
    static constexpr std::size_t CTRL_BYTES = STAMPED ? CMSG_SPACE(sizeof(scm_timestamping)) : 0;

   public:
    /// Throws if its rings cannot be mapped.
    KernelTcpStream() = default;

    ~KernelTcpStream() {
        if (fd_ >= 0) ::close(fd_);
    }

    KernelTcpStream(KernelTcpStream&& other) noexcept
        : rx_(std::move(other.rx_)),
          fd_(std::exchange(other.fd_, -1)),
          rx_ts_(other.rx_ts_),
          status_(other.status_),
          tx_(std::move(other.tx_)),
          inflight_(std::move(other.inflight_)),
          inflight_sent_(other.inflight_sent_) {}

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
        if constexpr (STAMPED) {
            const int flags = STAMP_FLAGS;
            if (::setsockopt(fd_, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof flags) != 0)
                return fail();
        }
        if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0)
            return fail();
        const int flags = ::fcntl(fd_, F_GETFL);
        if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) != 0) return fail();
        return status_ = StreamStatus::Open;
    }

    Readable read(std::size_t n) noexcept {
        // Only an empty receive ring receives, so every lend comes from one recv and one set of
        // stamps.
        if (rx_.unlent() == 0 && status_ == StreamStatus::Open) receive();
        return {rx_.lend(n), rx_ts_, status_};
    }

    /// Sends what is waiting first. Full when max bytes do not fit, Error when max exceeds
    /// TxSize.
    template <class F>
        requires std::is_invocable_r_v<std::size_t, F&, std::span<std::byte>>
    Written write(std::size_t max,
                  F&& fill) noexcept(std::is_nothrow_invocable_v<F&, std::span<std::byte>>) {
        if (status_ != StreamStatus::Open) return {0, status_};
        if (max > TxSize) return {0, status_ = StreamStatus::Error};
        send_pending();
        if (status_ != StreamStatus::Open) return {0, status_};
        bool              fits = false;
        const std::size_t n    = tx_.write([&](std::span<std::byte> space) noexcept(
                                            std::is_nothrow_invocable_v<F&, std::span<std::byte>>) {
            fits = space.size() >= max;
            return fits ? fill(space.first(max)) : std::size_t{0};
        });
        if (!fits) return {0, StreamStatus::Full};
        assert(n <= max);
        send_pending();
        return {n, status_};
    }

    Flushed flush() noexcept {
        const std::size_t sent = status_ == StreamStatus::Open ? send_pending() : 0;
        return {sent, inflight_.bytes().size() - inflight_sent_ + tx_.unlent(), status_};
    }

    /// Drops unread and unsent bytes.
    void close() noexcept {
        if (fd_ >= 0) ::close(std::exchange(fd_, -1));
        rx_.drop_unlent();
        inflight_      = {};
        inflight_sent_ = 0;
        tx_.drop_unlent();
        rx_ts_  = {};
        status_ = StreamStatus::Closed;
    }

   private:
    StreamStatus fail() noexcept { return status_ = StreamStatus::Error; }

    void receive() noexcept {
        ssize_t got = -1;
        int     err = EAGAIN;
        rx_.write([&](std::span<std::byte> space) noexcept {
            // A zero length recv returns 0, which reads as a close.
            if (space.empty()) return std::size_t{0};
            if constexpr (STAMPED) {
                iovec  iov{space.data(), space.size()};
                msghdr msg{};
                msg.msg_iov        = &iov;
                msg.msg_iovlen     = 1;
                msg.msg_control    = ctrl_.data();
                msg.msg_controllen = ctrl_.size();
                got                = ::recvmsg(fd_, &msg, 0);
                if (got > 0) stamp(msg);
            } else {
                got = ::recv(fd_, space.data(), space.size(), 0);
                if (got > 0) rx_ts_ = {.user = clock_ns(CLOCK_MONOTONIC)};
            }
            if (got > 0) return static_cast<std::size_t>(got);
            err = errno;
            return std::size_t{0};
        });
        if (got == 0)
            status_ = StreamStatus::Closed;
        else if (got < 0 && err != EAGAIN && err != EWOULDBLOCK && err != EINTR)
            status_ = StreamStatus::Error;
    }

    // One send of the range out, or of everything waiting when none is. Returns the bytes the
    // kernel took.
    std::size_t send_pending() noexcept {
        if (inflight_.bytes().empty()) inflight_ = tx_.lend(tx_.unlent());
        const auto rest = inflight_.bytes().subspan(inflight_sent_);
        if (rest.empty()) return 0; // nothing waiting
        // MSG_NOSIGNAL turns a send to a closed peer into EPIPE, not SIGPIPE.
        const ssize_t sent = ::send(fd_, rest.data(), rest.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                status_ = StreamStatus::Error;
            return 0;
        }
        inflight_sent_ += static_cast<std::size_t>(sent);
        if (inflight_sent_ == inflight_.bytes().size()) {
            inflight_      = {};
            inflight_sent_ = 0;
        }
        return static_cast<std::size_t>(sent);
    }

    // The kernel stamps the newest bytes of a recv on the realtime clock. It moves onto the
    // monotonic one by the gap between the two clocks now.
    void stamp(msghdr& msg) noexcept {
        rx_ts_ = {.user = clock_ns(CLOCK_MONOTONIC)};
        for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_TIMESTAMPING) continue;
            scm_timestamping t;
            std::memcpy(&t, CMSG_DATA(c), sizeof t);
            if constexpr (Stamps.kernel)
                if (const RxTime real = ns(t.ts[0]); real != 0)
                    rx_ts_.kernel = real - (clock_ns(CLOCK_REALTIME) - rx_ts_.user);
            if constexpr (Stamps.nic) rx_ts_.nic = ns(t.ts[2]);
            break;  // one per recv
        }
    }

    static RxTime ns(const timespec& ts) noexcept {
        return static_cast<RxTime>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
    }

    static RxTime clock_ns(clockid_t clock) noexcept {
        timespec ts{};
        ::clock_gettime(clock, &ts);
        return ns(ts);
    }

    LentMirrorRing<RxSize, MaxLent> rx_;
    int                             fd_ = -1;
    RxTimes                         rx_ts_{};
    StreamStatus                    status_ = StreamStatus::Closed;  // Open once connected

    alignas(cmsghdr) std::array<std::byte, CTRL_BYTES> ctrl_{};

    // inflight_ is the range being sent, declared after tx_ so it is handed back first.
    LentMirrorRing<TxSize, 1> tx_;
    LentBytes                 inflight_;
    std::size_t               inflight_sent_ = 0;  // bytes of inflight_ the kernel took
};

}  // namespace qp::network::transport::byte_stream
