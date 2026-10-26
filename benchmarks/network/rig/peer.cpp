// netbench_peer. The fixed far end every rung is measured against, sending to a receiver under
// test or receiving from a sender under test. Plain kernel sockets only.
// clang-format off
// linux/errqueue.h needs timespec first.
#include <time.h>
#include <linux/errqueue.h>
// clang-format on
#include <linux/net_tstamp.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "endpoint.hpp"
#include "message.hpp"
#include "receiver.hpp"
#include "report.hpp"
#include "stream_framer.hpp"
#include "traffic.hpp"

using namespace qp::network::rig;

namespace {

struct PeerArgs {
    bool          receive = false;  // receives from a sender under test, sends otherwise
    bool          tcp     = false;
    Endpoint      target;  // listened on for tcp, sent to or bound for udp
    Endpoint      ack;     // bound when sending, where acks go when receiving
    Traffic       traffic;
    std::uint32_t sender = 0;  // this peer's id when several send to one receiver
    std::string   out;
};

std::optional<PeerArgs> parse_args(int argc, char** argv) {
    PeerArgs a;
    bool     ok = argc % 2 == 1, have_proto = false, have_target = false, have_ack = false;
    for (int i = 1; ok && i + 1 < argc; i += 2) {
        const std::string_view key = argv[i], value = argv[i + 1];
        if (parse_traffic(key, value, a.traffic, ok)) continue;
        if (key == "--role") {
            ok        = value == "send" || value == "receive";
            a.receive = value == "receive";
        } else if (key == "--proto") {
            ok         = value == "tcp" || value == "udp";
            a.tcp      = value == "tcp";
            have_proto = true;
        } else if (key == "--target" || key == "--ack") {
            const auto e = parse_endpoint(value);
            ok           = e.has_value();
            if (ok) (key == "--target" ? a.target : a.ack) = *e;
            (key == "--target" ? have_target : have_ack) = true;
        } else if (key == "--sender") {
            ok = parse_number(value, a.sender) && a.sender < MAX_PEERS;
        } else if (key == "--out") {
            a.out = value;
        } else {
            ok = false;
        }
    }
    // One tcp connection carries one peer.
    if (ok && a.tcp && a.sender != 0) ok = false;
    if (ok && have_proto && have_target && have_ack) return a;
    std::fprintf(stderr,
                 "usage: %s [--role send|receive] --proto tcp|udp --target ip:port --ack ip:port\n"
                 "%s  [--sender n] [--out file]\n",
                 argv[0], TRAFFIC_USAGE);
    return std::nullopt;
}

// A TCP connection, accepted within ten seconds. -1 on failure.
int accept_one(const Endpoint& at) {
    const int lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    const int on  = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    const auto addr = to_sockaddr(at);
    int        fd   = -1;
    if (addr && ::bind(lfd, reinterpret_cast<const sockaddr*>(&*addr), sizeof *addr) == 0 &&
        ::listen(lfd, 1) == 0) {
        pollfd p{lfd, POLLIN, 0};
        if (::poll(&p, 1, 10'000) == 1) fd = ::accept4(lfd, nullptr, nullptr, SOCK_CLOEXEC);
    }
    ::close(lfd);
    // Each message leaves as soon as it is written.
    if (fd >= 0) ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
    return fd;
}

// Sends lens.size() messages laid back to back in bytes, as one datagram each. False when any
// was not sent.
bool send_datagrams(int fd, std::span<std::byte> bytes, std::span<const std::uint32_t> lens,
                    std::vector<iovec>& iov, std::vector<mmsghdr>& hdrs) {
    std::size_t at = 0;
    for (std::size_t k = 0; k < lens.size(); ++k) {
        iov[k]                     = {bytes.data() + at, lens[k]};
        hdrs[k].msg_hdr            = {};
        hdrs[k].msg_hdr.msg_iov    = &iov[k];
        hdrs[k].msg_hdr.msg_iovlen = 1;
        at += lens[k];
    }
    for (std::size_t sent = 0; sent < lens.size();) {
        const int n =
            ::sendmmsg(fd, hdrs.data() + sent, static_cast<unsigned>(lens.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool send_all(int fd, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const ssize_t n = ::send(fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
        if (n <= 0) return false;
        bytes = bytes.subspan(static_cast<std::size_t>(n));
    }
    return true;
}

int send_role(const PeerArgs& a) {
    const Traffic&      t       = a.traffic;
    const Schedule      plan    = make_schedule(t);
    const std::uint64_t total   = plan.sizes.size();
    const auto          largest = std::ranges::max(t.sizes, {}, &SizeWeight::size).size;
    // A batch of messages back to back.
    std::vector<std::byte>     buf(largest * t.batch);
    std::vector<std::uint32_t> lens(t.batch);
    std::vector<iovec>         iov(t.batch);
    std::vector<mmsghdr>       hdrs(t.batch);

    const int ack = udp_socket(&a.ack, nullptr);
    const int fd  = a.tcp ? accept_one(a.target) : udp_socket(nullptr, &a.target);
    if (ack < 0 || fd < 0) {
        std::perror("netbench_peer: socket");
        return 1;
    }
    set_timeout(ack, 10'000);
    if (!await_ack(ack, READY)) {
        std::fprintf(stderr, "netbench_peer: receiver never became ready\n");
        return 1;
    }
    set_timeout(ack, t.ack_ms);

    Pacer     pace(t, plan);
    SendStats st;
    pace.start();
    for (std::uint64_t i = 0; i < total;) {
        pace.wait(i);
        const std::int64_t  send_ts = mono_ns();
        const std::uint64_t n       = pace.batch_from(i, send_ts);
        std::size_t         bytes   = 0;
        for (std::uint64_t k = 0; k < n; ++k) {
            const std::uint64_t seq   = i + k;
            const std::uint32_t size  = plan.sizes[seq];
            const std::uint32_t flags = with_sender(
                (seq < t.warmup ? WARMUP : 0) | (t.mode == Mode::Ack ? ACK : 0), a.sender);
            write_header(std::span(buf).subspan(bytes, size), {size, flags, seq, send_ts});
            lens[k] = size;
            bytes += size;
        }
        const bool sent = a.tcp ? send_all(fd, std::span(buf).first(bytes))
                                : send_datagrams(fd, buf, std::span(lens).first(n), iov, hdrs);
        if (!sent) ++st.send_errors;
        if (t.mode == Mode::Ack && !await_ack(ack, i)) ++st.missed_acks;
        ++st.sends;
        i += n;
    }
    st.elapsed_ns = pace.elapsed();
    st.behind     = pace.behind();
    st.worst_late = pace.worst_late();

    // Sent a few times on udp, apart so a receiver still draining a flood has room for one.
    const auto end_msg = std::span(buf).first(sizeof(MessageHeader));
    write_header(end_msg, {sizeof(MessageHeader), with_sender(END, a.sender), total, mono_ns()});
    for (int i = 0; i < (a.tcp ? 1 : 3); ++i) {
        if (i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        send_all(fd, end_msg);
    }
    ::close(fd);
    ::close(ack);
    write_send_report(a.out, a.tcp ? "tcp" : "udp", t, st, nullptr);
    return 0;
}

constexpr int STAMP_FLAGS = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;

std::int64_t clock_ns(clockid_t clock) {
    timespec ts{};
    ::clock_gettime(clock, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

// The kernel's receive stamp in msg, on the realtime clock, 0 when it has none.
std::int64_t kernel_stamp(msghdr& msg) {
    for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_TIMESTAMPING) continue;
        scm_timestamping t;
        std::memcpy(&t, CMSG_DATA(c), sizeof t);
        return static_cast<std::int64_t>(t.ts[0].tv_sec) * 1'000'000'000 + t.ts[0].tv_nsec;
    }
    return 0;
}

// The receive times of bytes the kernel stamped at real, user taken now.
qp::network::transport::RxTimes stamps(std::int64_t real) {
    const std::int64_t user = clock_ns(CLOCK_MONOTONIC);
    // The kernel stamps on the realtime clock, moved onto the monotonic one by the gap now.
    const std::int64_t gap = clock_ns(CLOCK_REALTIME) - user;
    return {.nic = 0, .kernel = real ? real - gap : 0, .user = user};
}

// Room for one receive's stamps.
struct alignas(cmsghdr) Ctrl {
    std::byte bytes[CMSG_SPACE(sizeof(scm_timestamping))];
};

constexpr std::int64_t IDLE_NS = 2'000'000'000;

int receive_udp(const PeerArgs& a) {
    constexpr std::size_t BATCH = 32;
    const int             fd    = udp_socket(&a.target, nullptr);
    const int             ack   = udp_socket(nullptr, nullptr);
    const auto            base  = to_sockaddr(a.ack);
    if (fd < 0 || ack < 0 || !base ||
        ::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &STAMP_FLAGS, sizeof STAMP_FLAGS) != 0) {
        std::perror("netbench_peer: socket");
        return 1;
    }
    std::vector<std::byte>     buf(BATCH * MAX_MESSAGE);
    std::array<mmsghdr, BATCH> hdrs{};
    std::array<iovec, BATCH>   iov{};
    std::array<Ctrl, BATCH>    ctrl{};

    Recorder rec(ack, *base, 1);
    rec.ready();
    while (!rec.done()) {
        rec.nudge();
        for (std::size_t k = 0; k < BATCH; ++k) {
            iov[k]                         = {buf.data() + k * MAX_MESSAGE, MAX_MESSAGE};
            hdrs[k].msg_hdr                = {};
            hdrs[k].msg_hdr.msg_iov        = &iov[k];
            hdrs[k].msg_hdr.msg_iovlen     = 1;
            hdrs[k].msg_hdr.msg_control    = ctrl[k].bytes;
            hdrs[k].msg_hdr.msg_controllen = sizeof ctrl[k].bytes;
        }
        const int n = ::recvmmsg(fd, hdrs.data(), BATCH, MSG_DONTWAIT, nullptr);
        if (n <= 0) {
            if (rec.idle(IDLE_NS)) break;
            continue;
        }
        for (int k = 0; k < n; ++k) {
            const auto bytes =
                std::span(buf).subspan(static_cast<std::size_t>(k) * MAX_MESSAGE, hdrs[k].msg_len);
            if (bytes.size() < sizeof(MessageHeader)) {
                rec.bad_size();
                continue;
            }
            const MessageHeader h = read_header(bytes);
            if (h.size != bytes.size()) {
                rec.bad_size();
                continue;
            }
            rec.on_message(h, stamps(kernel_stamp(hdrs[k].msg_hdr)), mono_ns());
        }
        rec.on_read(static_cast<std::size_t>(n));
    }
    write_report(a.out, "peer_udp", "kernel", rec.done(), rec);
    ::close(fd);
    ::close(ack);
    return rec.done() ? 0 : 1;
}

int receive_tcp(const PeerArgs& a) {
    const int  fd   = accept_one(a.target);
    const int  ack  = udp_socket(nullptr, nullptr);
    const auto base = to_sockaddr(a.ack);
    if (fd < 0 || ack < 0 || !base ||
        ::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &STAMP_FLAGS, sizeof STAMP_FLAGS) != 0) {
        std::perror("netbench_peer: socket");
        return 1;
    }
    std::vector<std::byte> buf(1 << 20);
    Ctrl                   ctrl{};
    StreamFramer           framer;

    Recorder rec(ack, *base, 1);
    rec.ready();
    while (!rec.done()) {
        rec.nudge();
        iovec  iov{buf.data(), buf.size()};
        msghdr msg{};
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_control    = ctrl.bytes;
        msg.msg_controllen = sizeof ctrl.bytes;
        const ssize_t got  = ::recvmsg(fd, &msg, MSG_DONTWAIT);
        if (got == 0) break;
        if (got < 0) {
            if ((errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) || rec.idle(IDLE_NS))
                break;
            continue;
        }
        const qp::network::transport::RxTimes t = stamps(kernel_stamp(msg));
        const auto                            n =
            framer.feed(std::span(buf).first(static_cast<std::size_t>(got)),
                        [&](const MessageHeader& h) { rec.on_message(h, t, mono_ns()); });
        if (!n) {
            rec.bad_framing();
            break;
        }
        rec.on_read(*n);
    }
    write_report(a.out, "peer_tcp", "kernel", rec.done(), rec);
    ::close(fd);
    ::close(ack);
    return rec.done() ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const auto parsed = parse_args(argc, argv);
    if (!parsed) return 2;
    if (!parsed->receive) return send_role(*parsed);
    return parsed->tcp ? receive_tcp(*parsed) : receive_udp(*parsed);
}
