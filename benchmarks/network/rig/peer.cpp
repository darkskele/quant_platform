// netbench_peer. The fixed sender every rung is measured against. Plain kernel sockets only.
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "endpoint.hpp"
#include "message.hpp"

using namespace qp::network::rig;

namespace {

// How sends are spaced. ack waits for each message's ack, flood never waits.
enum class Mode { Ack, Fixed, Poisson, Burst, Flood };

struct SizeWeight {
    std::uint32_t size   = 0;
    double        weight = 1;
};

struct PeerArgs {
    bool                    tcp = false;
    Endpoint                target;  // listened on for tcp, sent to for udp
    Endpoint                ack;     // bound, the receiver's acks arrive here
    Mode                    mode      = Mode::Ack;
    std::string             mode_name = "ack";
    std::vector<SizeWeight> sizes{{64, 1}};
    std::string             sizes_text = "64";
    std::uint64_t           count      = 100'000;
    std::uint64_t           warmup     = 10'000;
    std::uint64_t           rate   = 10'000;  // messages per second, bursts per second for burst
    std::uint64_t           burst  = 8;
    std::uint64_t           batch  = 1;  // most messages one send call takes
    std::uint32_t           sender = 0;  // this peer's id when several send to one receiver
    std::uint64_t           seed   = 1;
    int                     ack_ms = 100;
    std::string             out;
};

// A send this late against its schedule counts as behind.
constexpr std::int64_t LATE_NS = 10'000;

template <class T>
bool parse_number(std::string_view text, T& out) {
    const auto [end, err] = std::from_chars(text.data(), text.data() + text.size(), out);
    return err == std::errc{} && end == text.data() + text.size();
}

// size or size:weight, comma separated.
bool parse_sizes(std::string_view text, std::vector<SizeWeight>& out) {
    out.clear();
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto item  = text.substr(0, comma);
        text             = comma == std::string_view::npos ? "" : text.substr(comma + 1);
        const auto colon = item.find(':');
        SizeWeight sw;
        if (!parse_number(item.substr(0, colon), sw.size) || sw.size < sizeof(MessageHeader) ||
            sw.size > MAX_MESSAGE)
            return false;
        if (colon != std::string_view::npos) {
            std::uint32_t w = 0;
            if (!parse_number(item.substr(colon + 1), w) || w == 0) return false;
            sw.weight = w;
        }
        out.push_back(sw);
    }
    return !out.empty();
}

std::optional<PeerArgs> parse_args(int argc, char** argv) {
    PeerArgs a;
    bool     ok = argc % 2 == 1, have_proto = false, have_target = false, have_ack = false;
    for (int i = 1; ok && i + 1 < argc; i += 2) {
        const std::string_view key = argv[i], value = argv[i + 1];
        if (key == "--proto") {
            ok         = value == "tcp" || value == "udp";
            a.tcp      = value == "tcp";
            have_proto = true;
        } else if (key == "--target" || key == "--ack") {
            const auto e = parse_endpoint(value);
            ok           = e.has_value();
            if (ok) (key == "--target" ? a.target : a.ack) = *e;
            (key == "--target" ? have_target : have_ack) = true;
        } else if (key == "--mode") {
            const std::pair<std::string_view, Mode> modes[] = {{"ack", Mode::Ack},
                                                               {"fixed", Mode::Fixed},
                                                               {"poisson", Mode::Poisson},
                                                               {"burst", Mode::Burst},
                                                               {"flood", Mode::Flood}};
            const auto                              m =
                std::ranges::find(modes, value, &std::pair<std::string_view, Mode>::first);
            ok = m != std::end(modes);
            if (ok) a.mode = m->second;
            a.mode_name = value;
        } else if (key == "--sizes") {
            ok           = parse_sizes(value, a.sizes);
            a.sizes_text = value;
        } else if (key == "--count") {
            ok = parse_number(value, a.count);
        } else if (key == "--warmup") {
            ok = parse_number(value, a.warmup);
        } else if (key == "--rate") {
            ok = parse_number(value, a.rate) && a.rate > 0;
        } else if (key == "--burst") {
            ok = parse_number(value, a.burst) && a.burst > 0;
        } else if (key == "--sender") {
            ok = parse_number(value, a.sender) && a.sender < MAX_PEERS;
        } else if (key == "--batch") {
            ok = parse_number(value, a.batch) && a.batch > 0 && a.batch <= 1024;
        } else if (key == "--seed") {
            ok = parse_number(value, a.seed);
        } else if (key == "--ack-ms") {
            ok = parse_number(value, a.ack_ms) && a.ack_ms > 0;
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
                 "usage: %s --proto tcp|udp --target ip:port --ack ip:port\n"
                 "  [--mode ack|fixed|poisson|burst|flood] [--rate per_second] [--burst n]\n"
                 "  [--sizes size[:weight],...] [--batch n] [--sender n] [--count n] [--warmup n]\n"
                 "  [--seed n] [--ack-ms n]\n"
                 "  [--out file]\n",
                 argv[0]);
    return std::nullopt;
}

// Every message's size and send time from the start, drawn once from the seed so each run sends
// the same traffic.
struct Schedule {
    std::vector<std::uint32_t> sizes;
    std::vector<std::int64_t>  due_ns;
};

Schedule make_schedule(const PeerArgs& a) {
    const std::uint64_t total = a.warmup + a.count;
    Schedule            s{std::vector<std::uint32_t>(total), std::vector<std::int64_t>(total, 0)};
    std::mt19937_64     rng(a.seed);

    std::vector<double> weights;
    for (const auto& sw : a.sizes) weights.push_back(sw.weight);
    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
    for (auto& size : s.sizes) size = a.sizes[pick(rng)].size;

    const double                          gap_ns = 1e9 / static_cast<double>(a.rate);
    std::exponential_distribution<double> poisson_gap(1 / gap_ns);
    double                                t = 0;
    for (std::uint64_t i = 0; i < total; ++i) {
        switch (a.mode) {
            case Mode::Fixed:
                t = static_cast<double>(i) * gap_ns;
                break;
            case Mode::Poisson:
                t += i ? poisson_gap(rng) : 0;
                break;
            case Mode::Burst:
                t = static_cast<double>(i / a.burst) * gap_ns;
                break;
            case Mode::Ack:
            case Mode::Flood:
                break;
        }
        s.due_ns[i] = static_cast<std::int64_t>(t);
    }
    return s;
}

void set_timeout(int fd, int ms) {
    const timeval tv{ms / 1000, (ms % 1000) * 1000};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

// Waits for the ack carrying seq, skipping older ones. False on a timeout.
bool await_ack(int ack, std::uint64_t seq) {
    for (;;) {
        std::uint64_t got = 0;
        if (::recv(ack, &got, sizeof got, 0) != sizeof got) return false;
        if (got == seq) return true;
    }
}

// A TCP connection from the receiver, accepted within ten seconds. -1 on failure.
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

}  // namespace

int main(int argc, char** argv) {
    const auto parsed = parse_args(argc, argv);
    if (!parsed) return 2;
    const PeerArgs& a = *parsed;

    const Schedule      plan    = make_schedule(a);
    const std::uint64_t total   = plan.sizes.size();
    const auto          largest = std::ranges::max(a.sizes, {}, &SizeWeight::size).size;
    // A batch of messages back to back.
    std::vector<std::byte>     buf(largest * a.batch);
    std::vector<std::uint32_t> lens(a.batch);
    std::vector<iovec>         iov(a.batch);
    std::vector<mmsghdr>       hdrs(a.batch);

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
    set_timeout(ack, a.ack_ms);

    const bool    paced = a.mode == Mode::Fixed || a.mode == Mode::Poisson || a.mode == Mode::Burst;
    std::uint64_t behind = 0, missed_acks = 0, send_errors = 0, sends = 0;
    std::int64_t  worst_late = 0;
    const std::int64_t start = mono_ns();

    for (std::uint64_t i = 0; i < total;) {
        if (paced) {
            const std::int64_t due = start + plan.due_ns[i];
            std::int64_t       now = mono_ns();
            while (now < due) now = mono_ns();
            // A burst is due at its first message. The rest follow it back to back.
            if (a.mode != Mode::Burst || i % a.burst == 0) {
                worst_late = std::max(worst_late, now - due);
                if (now - due > LATE_NS) ++behind;
            }
        }
        // Everything already due goes in one send, up to batch. Each waits for its own ack in ack
        // mode, so never batches.
        const std::int64_t send_ts = mono_ns();
        std::uint64_t      n       = 0;
        std::size_t        bytes   = 0;
        do {
            const std::uint64_t seq   = i + n;
            const std::uint32_t size  = plan.sizes[seq];
            const std::uint32_t flags = with_sender(
                (seq < a.warmup ? WARMUP : 0) | (a.mode == Mode::Ack ? ACK : 0), a.sender);
            write_header(std::span(buf).subspan(bytes, size), {size, flags, seq, send_ts});
            lens[n++] = size;
            bytes += size;
        } while (n < a.batch && i + n < total && a.mode != Mode::Ack &&
                 (!paced || start + plan.due_ns[i + n] <= send_ts));

        const bool sent = a.tcp ? send_all(fd, std::span(buf).first(bytes))
                                : send_datagrams(fd, buf, std::span(lens).first(n), iov, hdrs);
        if (!sent) ++send_errors;
        if (a.mode == Mode::Ack && !await_ack(ack, i)) ++missed_acks;
        ++sends;
        i += n;
    }
    const std::int64_t elapsed = mono_ns() - start;

    // Sent a few times on udp, apart so a receiver still draining a flood has room for one.
    const auto end_msg = std::span(buf).first(sizeof(MessageHeader));
    write_header(end_msg, {sizeof(MessageHeader), with_sender(END, a.sender), total, mono_ns()});
    for (int i = 0; i < (a.tcp ? 1 : 3); ++i) {
        if (i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        send_all(fd, end_msg);
    }
    ::close(fd);
    ::close(ack);

    std::FILE* f = a.out.empty() ? stdout : std::fopen(a.out.c_str(), "w");
    if (!f) {
        std::perror(a.out.c_str());
        return 1;
    }
    std::fprintf(f,
                 "{\"proto\": \"%s\", \"mode\": \"%s\", \"sizes\": \"%s\", \"rate\": %llu, "
                 "\"burst\": %llu, \"batch\": %llu, \"sends\": %llu, \"seed\": %llu, \"count\": "
                 "%llu, \"warmup\": %llu, "
                 "\"elapsed_ns\": %lld, \"behind\": %llu, \"worst_late_ns\": %lld, "
                 "\"missed_acks\": %llu, \"send_errors\": %llu}\n",
                 a.tcp ? "tcp" : "udp", a.mode_name.c_str(), a.sizes_text.c_str(),
                 static_cast<unsigned long long>(a.rate), static_cast<unsigned long long>(a.burst),
                 static_cast<unsigned long long>(a.batch), static_cast<unsigned long long>(sends),
                 static_cast<unsigned long long>(a.seed), static_cast<unsigned long long>(a.count),
                 static_cast<unsigned long long>(a.warmup), static_cast<long long>(elapsed),
                 static_cast<unsigned long long>(behind), static_cast<long long>(worst_late),
                 static_cast<unsigned long long>(missed_acks),
                 static_cast<unsigned long long>(send_errors));
    if (f != stdout) std::fclose(f);
    return 0;
}
