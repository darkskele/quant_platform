#pragma once
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#include "byte_stream.hpp"
#include "datagram_channel.hpp"
#include "endpoint.hpp"
#include "histogram.hpp"
#include "message.hpp"
#include "report.hpp"
#include "traffic.hpp"

namespace qp::network::rig {

struct SenderArgs {
    Endpoint    target;  ///< The peer receiving.
    Endpoint    ack;     ///< Bound, where the peer's acks arrive.
    Traffic     traffic;
    std::string out;
};

/// argv[0] names the program, the rest are the sender's arguments.
inline std::optional<SenderArgs> parse_sender_args(int argc, char** argv) {
    SenderArgs a;
    bool       ok = argc % 2 == 1, have_target = false, have_ack = false;
    for (int i = 1; ok && i + 1 < argc; i += 2) {
        const std::string_view key = argv[i], value = argv[i + 1];
        if (parse_traffic(key, value, a.traffic, ok)) continue;
        if (key == "--target" || key == "--ack") {
            const auto e = parse_endpoint(value);
            ok           = e.has_value();
            if (ok) (key == "--target" ? a.target : a.ack) = *e;
            (key == "--target" ? have_target : have_ack) = true;
        } else if (key == "--out") {
            a.out = value;
        } else {
            ok = false;
        }
    }
    if (ok && have_target && have_ack) return a;
    std::fprintf(stderr, "usage: %s send --target ip:port --ack ip:port\n%s  [--out file]\n",
                 argv[0], TRAFFIC_USAGE);
    return std::nullopt;
}

/// Writes messages i to i + n - 1 back to back into space, stamped send_ts. Only headers are
/// written, the rest of each message is whatever the space held.
inline void fill_messages(std::span<std::byte> space, const Schedule& plan, const Traffic& t,
                          std::uint64_t i, std::uint64_t n, std::int64_t send_ts) noexcept {
    std::size_t at = 0;
    for (std::uint64_t seq = i; seq < i + n; ++seq) {
        const std::uint32_t size  = plan.sizes[seq];
        const std::uint32_t flags = (seq < t.warmup ? WARMUP : 0) | (t.mode == Mode::Ack ? ACK : 0);
        write_header(space.subspan(at, size), {size, flags, seq, send_ts});
        at += size;
    }
}

inline std::size_t batch_bytes(const Schedule& plan, std::uint64_t i, std::uint64_t n) noexcept {
    std::size_t bytes = 0;
    for (std::uint64_t seq = i; seq < i + n; ++seq) bytes += plan.sizes[seq];
    return bytes;
}

/// Sends the schedule. send(i, n, send_ts) hands messages i to i + n - 1 to the backend, waiting
/// out Full, and returns false when the backend failed. Records how long each send held the
/// caller.
template <class Send>
int drive(const SenderArgs& a, std::string_view proto, int ack, Send&& send, SendStats& st,
          Histogram& write_call) {
    const Traffic& t     = a.traffic;
    const Schedule plan  = make_schedule(t);
    const auto     total = plan.sizes.size();

    set_timeout(ack, 10'000);
    if (!await_ack(ack, READY)) {
        std::fprintf(stderr, "%.*s sender: receiver never became ready\n",
                     static_cast<int>(proto.size()), proto.data());
        return 1;
    }
    set_timeout(ack, t.ack_ms);

    Pacer pace(t, plan);
    pace.start();
    for (std::uint64_t i = 0; i < total;) {
        pace.wait(i);
        const std::int64_t  send_ts = mono_ns();
        const std::uint64_t n       = pace.batch_from(i, send_ts);
        const bool          ok      = send(plan, i, n, send_ts);
        write_call.record(mono_ns() - send_ts);
        ++st.sends;
        if (!ok) {
            ++st.send_errors;
            break;
        }
        if (t.mode == Mode::Ack && !await_ack(ack, i)) ++st.missed_acks;
        i += n;
    }
    st.elapsed_ns = pace.elapsed();
    st.behind     = pace.behind();
    st.worst_late = pace.worst_late();
    return 0;
}

/// Connects to the peer and sends the schedule through a byte stream, a batch per write.
template <transport::byte_stream::ByteStream S>
int run_stream_sender(const SenderArgs& a, std::string_view backend) {
    using transport::byte_stream::StreamStatus;
    const int ack = udp_socket(&a.ack, nullptr);
    if (ack < 0) {
        std::perror("ack socket");
        return 1;
    }
    S s;
    // The peer may not be listening yet.
    const std::int64_t give_up = mono_ns() + 5'000'000'000;
    while (s.connect(a.target.ip, a.target.port) != StreamStatus::Open) {
        if (mono_ns() > give_up) {
            std::fprintf(stderr, "%.*s: connect failed\n", static_cast<int>(backend.size()),
                         backend.data());
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const Traffic& t = a.traffic;
    SendStats      st;
    Histogram      write_call;
    auto send = [&](const Schedule& plan, std::uint64_t i, std::uint64_t n, std::int64_t send_ts) {
        const std::size_t bytes = batch_bytes(plan, i, n);
        auto              fill  = [&](std::span<std::byte> space) noexcept {
            fill_messages(space, plan, t, i, n, send_ts);
            return bytes;
        };
        auto w = s.write(bytes, fill);
        if (w.status == StreamStatus::Full) ++st.full_waits;
        while (w.status == StreamStatus::Full) {
            s.flush();
            w = s.write(bytes, fill);
        }
        return w.status == StreamStatus::Open;
    };
    const int rc = drive(a, "tcp", ack, send, st, write_call);

    // The end, then everything waiting out before the stream closes.
    std::byte end[sizeof(MessageHeader)];
    write_header(end, {sizeof(MessageHeader), END, t.warmup + t.count, mono_ns()});
    s.write(sizeof end, [&](std::span<std::byte> space) noexcept {
        std::ranges::copy(end, space.begin());
        return sizeof end;
    });
    const std::int64_t drain_until = mono_ns() + 5'000'000'000;
    while (s.flush().pending > 0 && mono_ns() < drain_until) {
    }
    write_send_report(a.out, backend, t, st, &write_call);
    ::close(ack);
    return rc;
}

/// Targets the peer and sends the schedule through a datagram channel, a write per message.
template <transport::datagram::DatagramChannel C>
int run_channel_sender(const SenderArgs& a, std::string_view backend) {
    using transport::datagram::ChannelStatus;
    const int ack = udp_socket(&a.ack, nullptr);
    if (ack < 0) {
        std::perror("ack socket");
        return 1;
    }
    C c;
    if (c.target(a.target.ip, a.target.port) != ChannelStatus::Open) {
        std::fprintf(stderr, "%.*s: target failed\n", static_cast<int>(backend.size()),
                     backend.data());
        return 1;
    }

    const Traffic& t = a.traffic;
    SendStats      st;
    Histogram      write_call;
    auto send = [&](const Schedule& plan, std::uint64_t i, std::uint64_t n, std::int64_t send_ts) {
        for (std::uint64_t seq = i; seq < i + n; ++seq) {
            const std::size_t size = plan.sizes[seq];
            auto              fill = [&](std::span<std::byte> space) noexcept {
                fill_messages(space, plan, t, seq, 1, send_ts);
                return size;
            };
            auto w = c.write(size, fill);
            if (w.status == ChannelStatus::Full) ++st.full_waits;
            while (w.status == ChannelStatus::Full) {
                c.flush();
                w = c.write(size, fill);
            }
            if (w.status != ChannelStatus::Open) return false;
        }
        return true;
    };
    const int rc = drive(a, "udp", ack, send, st, write_call);

    // Sent a few times, apart so a receiver still draining a flood has room for one.
    for (int k = 0; k < 3; ++k) {
        if (k) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        c.write(sizeof(MessageHeader), [&](std::span<std::byte> space) noexcept {
            write_header(space, {sizeof(MessageHeader), END, t.warmup + t.count, mono_ns()});
            return sizeof(MessageHeader);
        });
        c.flush();
    }
    write_send_report(a.out, backend, t, st, &write_call);
    ::close(ack);
    return rc;
}

}  // namespace qp::network::rig
