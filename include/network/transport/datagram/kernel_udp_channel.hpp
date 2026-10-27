#pragma once
#include <arpa/inet.h>
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

#include "datagram_channel.hpp"
#include "lent_slot_pool.hpp"
#include "rx_times.hpp"

namespace qp::network::transport::datagram {

/// A DatagramChannel over a kernel UDP socket. One recvmmsg fills a batch of slots, then reads
/// lend them one at a time. Writes fill a slot of a second pool and send it, and datagrams the
/// kernel cannot take yet wait in their slots.
/// @tparam SlotSize slot bytes, the largest datagram kept or written, a multiple of 64.
/// @tparam SlotCount receive slots, lent and posted together.
/// @tparam Batch most datagrams one recvmmsg takes, at most SlotCount. Also the send slots, so
/// the most datagrams that can wait.
/// @tparam Stamps receive stamps taken besides the user one.
template <std::size_t SlotSize, std::size_t SlotCount, std::size_t Batch,
          RxStamps Stamps = RxStamps{}>
class KernelUdpChannel {
    static_assert(Batch > 0 && Batch <= SlotCount, "Batch must be between 1 and SlotCount");

    static constexpr bool STAMPED = Stamps.kernel || Stamps.nic;
    static constexpr int  STAMP_FLAGS =
        (Stamps.kernel ? SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE : 0) |
        (Stamps.nic ? SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE : 0);
    // Room for the drop count and the stamps one datagram brings.
    static constexpr std::size_t CTRL_BYTES =
        CMSG_SPACE(sizeof(std::uint32_t)) + (STAMPED ? CMSG_SPACE(sizeof(scm_timestamping)) : 0);

    // One datagram's control space, aligned for its first header.
    struct alignas(cmsghdr) Ctrl {
        std::byte bytes[CTRL_BYTES];
    };

    // A datagram received and not yet read.
    struct Ready {
        std::uint32_t slot = 0;
        std::uint32_t len  = 0;
        RxTimes       rx_ts{};
    };

    // A datagram written and not yet sent.
    struct Pending {
        std::uint32_t slot = 0;
        std::size_t   len  = 0;
    };

   public:
    /// Throws if its pool cannot be mapped.
    KernelUdpChannel() = default;

    ~KernelUdpChannel() { close_and_release(); }

    KernelUdpChannel(KernelUdpChannel&& other) noexcept
        : pool_(std::move(other.pool_)),
          fd_(std::exchange(other.fd_, -1)),
          status_(other.status_),
          port_(other.port_),
          dropped_(other.dropped_),
          truncated_(other.truncated_),
          posted_(other.posted_),
          posted_count_(std::exchange(other.posted_count_, 0)),
          ready_(other.ready_),
          ready_head_(std::exchange(other.ready_head_, 0)),
          ready_count_(std::exchange(other.ready_count_, 0)),
          tx_pool_(std::move(other.tx_pool_)),
          targeted_(std::exchange(other.targeted_, false)),
          pending_(other.pending_),
          pending_head_(std::exchange(other.pending_head_, 0)),
          pending_count_(std::exchange(other.pending_count_, 0)) {}

    KernelUdpChannel& operator=(KernelUdpChannel&&)      = delete;
    KernelUdpChannel(const KernelUdpChannel&)            = delete;
    KernelUdpChannel& operator=(const KernelUdpChannel&) = delete;

    /// Closes any current socket first. Port 0 takes a free one. Error on a bad address or a
    /// failed bind.
    ChannelStatus bind(std::string_view ip, std::uint16_t port) noexcept {
        close();
        sockaddr_in addr{};
        if (!to_addr(ip, port, addr) || !open_socket()) return fail();
        if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0) return fail();
        if (!learn_port()) return fail();
        return status_ = ChannelStatus::Open;
    }

    /// Opens a socket when none is, so a channel that only sends needs no bind. Sends what is
    /// waiting to the old target first, and is Full, keeping the old target, when that cannot
    /// finish. Error on a bad address.
    ChannelStatus target(std::string_view ip, std::uint16_t port) noexcept {
        sockaddr_in addr{};
        if (!to_addr(ip, port, addr)) return fail();
        if (fd_ < 0) {
            if (!open_socket()) return fail();
            status_ = ChannelStatus::Open;
        }
        if (status_ != ChannelStatus::Open) return status_;
        send_pending();
        if (status_ != ChannelStatus::Open) return status_;
        if (pending_count_ > 0) return ChannelStatus::Full;
        // A UDP connect only sets where sends go and which source reads accept.
        if (::connect(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0)
            return fail();
        targeted_ = true;
        if (port_ == 0 && !learn_port()) return fail();
        return status_;
    }

    Datagram read() noexcept {
        // Only receives once every datagram from the last batch is read, so a batch keeps order.
        if (ready_head_ == ready_count_ && status_ == ChannelStatus::Open) receive();
        if (ready_head_ == ready_count_) return {{}, {}, status_};
        const Ready& r = ready_[ready_head_++];
        return {pool_.lend(r.slot, 0, r.len), r.rx_ts, status_};
    }

    /// Sends what is waiting first. Full when no send slot is free, Error with no target or when
    /// max exceeds SlotSize.
    template <class F>
        requires std::is_invocable_r_v<std::size_t, F&, std::span<std::byte>>
    Written write(std::size_t max,
                  F&& fill) noexcept(std::is_nothrow_invocable_v<F&, std::span<std::byte>>) {
        if (status_ != ChannelStatus::Open) return {0, status_};
        if (!targeted_) return {0, ChannelStatus::Error};
        if (max > SlotSize) return {0, status_ = ChannelStatus::Error};
        send_pending();
        if (status_ != ChannelStatus::Open) return {0, status_};
        const auto slot = tx_pool_.acquire();
        if (!slot) return {0, ChannelStatus::Full};
        const std::size_t n = fill(tx_pool_.slot(*slot).first(max));
        assert(n <= max);
        pending_[(pending_head_ + pending_count_++) % Batch] = {*slot, n};
        send_pending();
        return {n, status_};
    }

    Flushed flush() noexcept {
        const std::size_t sent = status_ == ChannelStatus::Open ? send_pending() : 0;
        return {sent, pending_count_, status_};
    }

    /// As the kernel reports it with the next datagram that arrives after a drop.
    std::uint64_t dropped() const noexcept { return dropped_; }

    std::uint64_t truncated() const noexcept { return truncated_; }

    /// Drops unread and unsent datagrams and the target. Posted slots stay posted for the next
    /// bind.
    void close() noexcept {
        if (fd_ >= 0) ::close(std::exchange(fd_, -1));
        while (ready_head_ < ready_count_) pool_.release(ready_[ready_head_++].slot);
        ready_head_ = ready_count_ = 0;
        for (; pending_count_ > 0; --pending_count_, pending_head_ = (pending_head_ + 1) % Batch)
            tx_pool_.release(pending_[pending_head_].slot);
        pending_head_ = 0;
        targeted_     = false;
        port_         = 0;
        status_       = ChannelStatus::Closed;
    }

    /// The local port, 0 before bind or target.
    std::uint16_t local_port() const noexcept { return port_; }

   private:
    ChannelStatus fail() noexcept { return status_ = ChannelStatus::Error; }

    static bool to_addr(std::string_view ip, std::uint16_t port, sockaddr_in& addr) noexcept {
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        char text[INET_ADDRSTRLEN]{};
        if (ip.size() >= sizeof text) return false;
        std::ranges::copy(ip, text);
        return ::inet_pton(AF_INET, text, &addr.sin_addr) == 1;
    }

    bool open_socket() noexcept {
        fd_ = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd_ < 0) return false;
        const int on = 1;
        if (::setsockopt(fd_, SOL_SOCKET, SO_RXQ_OVFL, &on, sizeof on) != 0) return false;
        if constexpr (STAMPED) {
            const int flags = STAMP_FLAGS;
            if (::setsockopt(fd_, SOL_SOCKET, SO_TIMESTAMPING, &flags, sizeof flags) != 0)
                return false;
        }
        return true;
    }

    bool learn_port() noexcept {
        sockaddr_in addr{};
        socklen_t   len = sizeof addr;
        if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) return false;
        port_ = ntohs(addr.sin_port);
        return true;
    }

    // Sends waiting datagrams oldest first until the kernel takes no more. Returns how many went.
    std::size_t send_pending() noexcept {
        std::size_t sent = 0;
        while (pending_count_ > 0) {
            // The waiting run up to the ring's end goes in one call.
            const std::size_t run = std::min(pending_count_, Batch - pending_head_);
            for (std::size_t k = 0; k < run; ++k) {
                const Pending& p               = pending_[pending_head_ + k];
                tx_iov_[k]                     = {tx_pool_.slot(p.slot).data(), p.len};
                tx_hdrs_[k].msg_hdr            = {};
                tx_hdrs_[k].msg_hdr.msg_iov    = &tx_iov_[k];
                tx_hdrs_[k].msg_hdr.msg_iovlen = 1;
            }
            const int n =
                ::sendmmsg(fd_, tx_hdrs_.data(), static_cast<unsigned>(run), MSG_DONTWAIT);
            if (n < 0) {
                // ENOBUFS is a full device queue, worth a retry like a full socket.
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR && errno != ENOBUFS)
                    fail();
                break;
            }
            for (int k = 0; k < n; ++k) {
                tx_pool_.release(pending_[pending_head_].slot);
                pending_head_ = (pending_head_ + 1) % Batch;
                --pending_count_;
                ++sent;
            }
            if (static_cast<std::size_t>(n) < run) break;
        }
        return sent;
    }

    void close_and_release() noexcept {
        close();
        while (posted_count_ > 0) pool_.release(posted_[--posted_count_]);
    }

    void receive() noexcept {
        while (posted_count_ < Batch) {
            const auto s = pool_.acquire();
            if (!s) break;
            posted_[posted_count_++] = *s;
        }
        // No free slot, so arrivals wait in the socket until a lent one comes back.
        if (posted_count_ == 0) return;

        // Rebuilt every call, as recvmmsg overwrites the lengths.
        for (std::size_t i = 0; i < posted_count_; ++i) {
            iov_[i]                         = {pool_.slot(posted_[i]).data(), SlotSize};
            hdrs_[i].msg_hdr                = {};
            hdrs_[i].msg_hdr.msg_iov        = &iov_[i];
            hdrs_[i].msg_hdr.msg_iovlen     = 1;
            hdrs_[i].msg_hdr.msg_control    = ctrl_[i].bytes;
            hdrs_[i].msg_hdr.msg_controllen = CTRL_BYTES;
        }
        const int got = ::recvmmsg(fd_, hdrs_.data(), static_cast<unsigned>(posted_count_),
                                   MSG_DONTWAIT, nullptr);
        if (got <= 0) {
            if (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) fail();
            return;
        }

        const RxTime user = clock_ns(CLOCK_MONOTONIC);
        // Kernel stamps are on the realtime clock, moved onto the monotonic one by this gap.
        RxTime gap = 0;
        if constexpr (Stamps.kernel) gap = clock_ns(CLOCK_REALTIME) - user;

        ready_head_ = ready_count_ = 0;
        const auto n               = static_cast<std::size_t>(got);
        for (std::size_t i = 0; i < n; ++i) {
            msghdr& msg = hdrs_[i].msg_hdr;
            Ready   r{posted_[i], hdrs_[i].msg_len, {.user = user}};
            read_ctrl(msg, r.rx_ts, gap);
            if (msg.msg_flags & MSG_TRUNC) {
                pool_.release(r.slot);
                ++truncated_;
                continue;
            }
            ready_[ready_count_++] = r;
        }
        // Slots recvmmsg left empty move to the front, still posted.
        std::copy(posted_.begin() + n, posted_.begin() + posted_count_, posted_.begin());
        posted_count_ -= n;
    }

    void read_ctrl(msghdr& msg, RxTimes& rx_ts, RxTime gap) noexcept {
        for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level != SOL_SOCKET) continue;
            if (c->cmsg_type == SO_RXQ_OVFL) {
                // The socket's running drop count, 32 bits wide so it wraps.
                std::uint32_t drops;
                std::memcpy(&drops, CMSG_DATA(c), sizeof drops);
                dropped_ = drops;
            } else if constexpr (STAMPED) {
                if (c->cmsg_type != SCM_TIMESTAMPING) continue;
                scm_timestamping t;
                std::memcpy(&t, CMSG_DATA(c), sizeof t);
                if constexpr (Stamps.kernel)
                    if (const RxTime real = ns(t.ts[0]); real != 0) rx_ts.kernel = real - gap;
                if constexpr (Stamps.nic) rx_ts.nic = ns(t.ts[2]);
            }
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

    LentSlotPool<SlotSize, SlotCount> pool_;
    int                               fd_        = -1;
    ChannelStatus                     status_    = ChannelStatus::Closed;  // Open once bound
    std::uint16_t                     port_      = 0;
    std::uint64_t                     dropped_   = 0;
    std::uint64_t                     truncated_ = 0;

    // Slots handed to the kernel, filled front first.
    std::array<std::uint32_t, Batch> posted_{};
    std::size_t                      posted_count_ = 0;

    // The last batch, read from ready_head_ on.
    std::array<Ready, Batch> ready_{};
    std::size_t              ready_head_  = 0;
    std::size_t              ready_count_ = 0;

    // recvmmsg's arguments, pointed into the slots and the control space each call.
    std::array<mmsghdr, Batch> hdrs_{};
    std::array<iovec, Batch>   iov_{};
    std::array<Ctrl, Batch>    ctrl_{};

    // Send slots, and the datagrams waiting in them as a ring from pending_head_.
    LentSlotPool<SlotSize, Batch> tx_pool_;
    bool                          targeted_ = false;
    std::array<Pending, Batch>    pending_{};
    std::size_t                   pending_head_  = 0;
    std::size_t                   pending_count_ = 0;

    // sendmmsg's arguments, pointed into the send slots each call.
    std::array<mmsghdr, Batch> tx_hdrs_{};
    std::array<iovec, Batch>   tx_iov_{};
};

}  // namespace qp::network::transport::datagram
