#pragma once
#include <sys/socket.h>
#include <unistd.h>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "byte_stream.hpp"
#include "datagram_channel.hpp"
#include "endpoint.hpp"
#include "histogram.hpp"
#include "message.hpp"
#include "report.hpp"
#include "rx_times.hpp"
#include "stream_framer.hpp"

namespace qp::network::rig {

/// Per stage latencies and correctness counts for one receiver run, from one or more peers.
class Recorder {
   public:
    /// Peer k's acks go to ack_base's port plus k.
    Recorder(int ack_fd, const sockaddr_in& ack_base, std::uint32_t peers)
        : ack_fd_(ack_fd),
          last_rx_(mono_ns()),
          acks_(peers, ack_base),
          next_(peers),
          started_(peers),
          ended_(peers),
          unstarted_(peers) {
        for (std::uint32_t k = 0; k < peers; ++k)
            acks_[k].sin_port = htons(static_cast<std::uint16_t>(ntohs(ack_base.sin_port) + k));
    }

    /// Tells every peer that has not sent yet to start.
    void ready() noexcept {
        for (std::uint32_t k = 0; k < acks_.size(); ++k)
            if (!started_[k]) ack(k, READY);
        last_ready_ = mono_ns();
    }

    /// Call once per loop. Repeats ready() every 100 ms while a peer has not sent, in case it was
    /// not listening yet. Reads the clock once in 1024 calls, and only until every peer starts.
    void nudge() noexcept {
        if (unstarted_ == 0 || ++nudge_polls_ % 1024 != 0) return;
        if (mono_ns() - last_ready_ > 100'000'000) ready();
    }

    /// app is when the message was parsed.
    void on_message(const MessageHeader& h, const transport::RxTimes& t,
                    std::int64_t app) noexcept {
        const std::uint32_t sender = sender_of(h);
        if (sender >= acks_.size()) {
            ++bad_sender_;
            return;
        }
        if (!started_[sender]) {
            started_[sender] = true;
            --unstarted_;
        }
        if (h.flags & END) {
            if (!ended_[sender]) ++peers_ended_;
            ended_[sender] = true;
            return;
        }
        track(sender, h.seq);
        last_rx_ = t.user;
        if (!(h.flags & WARMUP)) {
            if (messages_ == 0) first_rx_ = t.user;
            last_msg_rx_ = t.user;
            ++messages_;
            send_to_user_.record(t.user - h.send_ts);
            send_to_app_.record(app - h.send_ts);
            user_to_app_.record(app - t.user);
            if (t.kernel != 0) {
                send_to_kernel_.record(t.kernel - h.send_ts);
                kernel_to_user_.record(t.user - t.kernel);
            }
        } else {
            ++warmup_;
        }
        if (h.flags & ACK) ack(sender, h.seq);
    }

    /// Messages one read or one batch of datagrams brought.
    void on_read(std::size_t messages) noexcept {
        per_read_.record(static_cast<std::int64_t>(messages));
    }

    void bad_framing() noexcept { ++bad_framing_; }

    void bad_size() noexcept { ++bad_size_; }

    void lost(std::uint64_t dropped, std::uint64_t truncated) noexcept {
        dropped_   = dropped;
        truncated_ = truncated;
    }

    /// Every peer has ended its run.
    bool done() const noexcept { return peers_ended_ == acks_.size(); }

    /// From the first recorded message reaching user space to the last.
    std::int64_t span_ns() const noexcept { return last_msg_rx_ - first_rx_; }

    /// True once nothing has arrived for limit_ns. Reads the clock once in 1024 calls.
    bool idle(std::int64_t limit_ns) noexcept {
        if (++idle_polls_ % 1024 != 0) return false;
        const std::int64_t now = mono_ns();
        return now - last_rx_ > limit_ns;
    }

    /// Calls f(name, histogram) for every stage.
    template <class F>
    void each_stage(F&& f) const {
        f("send_to_user", send_to_user_);
        f("send_to_app", send_to_app_);
        f("user_to_app", user_to_app_);
        f("send_to_kernel", send_to_kernel_);
        f("kernel_to_user", kernel_to_user_);
        f("messages_per_read", per_read_);
    }

    /// Calls f(name, count) for every counter.
    template <class F>
    void each_counter(F&& f) const {
        f("messages", messages_);
        f("warmup", warmup_);
        f("gaps", gaps_);
        f("late", late_);
        f("bad_framing", bad_framing_);
        f("bad_size", bad_size_);
        f("bad_sender", bad_sender_);
        f("dropped", dropped_);
        f("truncated", truncated_);
    }

   private:
    void ack(std::uint32_t peer, std::uint64_t seq) const noexcept {
        ::sendto(ack_fd_, &seq, sizeof seq, 0, reinterpret_cast<const sockaddr*>(&acks_[peer]),
                 sizeof acks_[peer]);
    }

    // Each peer numbers its own messages from 0.
    void track(std::uint32_t sender, std::uint64_t seq) noexcept {
        std::uint64_t& next = next_[sender];
        if (seq == next) {
            ++next;
        } else if (seq > next) {
            gaps_ += seq - next;
            next = seq + 1;
        } else {
            ++late_;
        }
    }

    int           ack_fd_;
    std::int64_t  last_rx_;
    std::int64_t  first_rx_    = 0;
    std::int64_t  last_msg_rx_ = 0;
    std::uint64_t idle_polls_  = 0;
    std::int64_t  last_ready_  = 0;
    std::uint64_t nudge_polls_ = 0;

    std::vector<sockaddr_in>   acks_;
    std::vector<std::uint64_t> next_;
    std::vector<bool>          started_;
    std::vector<bool>          ended_;
    std::size_t                unstarted_;
    std::size_t                peers_ended_ = 0;

    Histogram send_to_user_, send_to_app_, user_to_app_, send_to_kernel_, kernel_to_user_,
        per_read_;

    std::uint64_t messages_ = 0, warmup_ = 0, gaps_ = 0, late_ = 0, bad_framing_ = 0, bad_size_ = 0,
                  bad_sender_ = 0, dropped_ = 0, truncated_ = 0;
};

struct ReceiverArgs {
    Endpoint target;  ///< The peer to connect to on a stream, the address to bind on a channel.
    Endpoint ack;     ///< The first peer's ack socket, the next peers' on the ports after.
    std::uint32_t peers  = 1;
    std::string   stamps = "none";
    std::string   out;
    std::int64_t  idle_ns = 2'000'000'000;
};

inline std::optional<ReceiverArgs> parse_receiver_args(int argc, char** argv) {
    ReceiverArgs a;
    bool         ok = argc % 2 == 1, have_target = false, have_ack = false;
    for (int i = 1; ok && i + 1 < argc; i += 2) {
        const std::string_view key = argv[i], value = argv[i + 1];
        if (key == "--target" || key == "--ack") {
            const auto e = parse_endpoint(value);
            ok           = e.has_value();
            if (ok) (key == "--target" ? a.target : a.ack) = *e;
            (key == "--target" ? have_target : have_ack) = true;
        } else if (key == "--stamps") {
            ok       = value == "none" || value == "kernel";
            a.stamps = value;
        } else if (key == "--out") {
            a.out = value;
        } else if (key == "--peers") {
            const auto [end, err] =
                std::from_chars(value.data(), value.data() + value.size(), a.peers);
            ok = err == std::errc{} && end == value.data() + value.size() && a.peers > 0 &&
                 a.peers <= MAX_PEERS;
        } else if (key == "--idle-ms") {
            std::int64_t ms       = 0;
            const auto [end, err] = std::from_chars(value.data(), value.data() + value.size(), ms);
            ok        = err == std::errc{} && end == value.data() + value.size() && ms > 0;
            a.idle_ns = ms * 1'000'000;
        } else {
            ok = false;
        }
    }
    if (ok && have_target && have_ack) return a;
    std::fprintf(stderr,
                 "usage: %s --target ip:port --ack ip:port [--peers n] [--stamps none|kernel] "
                 "[--out file] [--idle-ms n]\n",
                 argv[0]);
    return std::nullopt;
}

/// The run as JSON, to path or to stdout when path is empty. Values are nanoseconds.
inline void write_report(const std::string& path, std::string_view backend, std::string_view stamps,
                         bool completed, const Recorder& rec) {
    std::FILE* f = path.empty() ? stdout : std::fopen(path.c_str(), "w");
    if (!f) {
        std::perror(path.c_str());
        return;
    }
    std::fprintf(f,
                 "{\"backend\": \"%.*s\", \"stamps\": \"%.*s\", \"completed\": %s, \"span_ns\": "
                 "%lld,\n",
                 static_cast<int>(backend.size()), backend.data(), static_cast<int>(stamps.size()),
                 stamps.data(), completed ? "true" : "false",
                 static_cast<long long>(rec.span_ns()));
    std::fprintf(f, " \"counters\": {");
    bool first = true;
    rec.each_counter([&](const char* name, std::uint64_t n) {
        std::fprintf(f, "%s\"%s\": %llu", first ? "" : ", ", name,
                     static_cast<unsigned long long>(n));
        first = false;
    });
    std::fprintf(f, "},\n \"stages\": {");
    first = true;
    rec.each_stage([&](const char* name, const Histogram& h) {
        std::fprintf(f, "%s\n  \"%s\": ", first ? "" : ",", name);
        write_histogram(f, h);
        first = false;
    });
    std::fprintf(f, "\n }\n}\n");
    if (f != stdout) std::fclose(f);
}

/// Connects to the peer, then reads framed messages until the peer ends the run.
template <transport::byte_stream::ByteStream S>
int run_stream_receiver(const ReceiverArgs& a, std::string_view backend) {
    using transport::byte_stream::StreamStatus;
    const int  ack      = udp_socket(nullptr, nullptr);
    const auto ack_base = to_sockaddr(a.ack);
    if (ack < 0 || !ack_base) {
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

    Recorder rec(ack, *ack_base, a.peers);
    rec.ready();
    StreamFramer framer;
    while (!rec.done()) {
        rec.nudge();
        const auto r = s.read(std::numeric_limits<std::size_t>::max());
        if (r.lent.bytes().empty()) {
            if (r.status != StreamStatus::Open || rec.idle(a.idle_ns)) break;
            continue;
        }
        const auto n = framer.feed(
            r.lent.bytes(), [&](const MessageHeader& h) { rec.on_message(h, r.rx_ts, mono_ns()); });
        if (!n) {
            rec.bad_framing();
            break;
        }
        rec.on_read(*n);
    }
    write_report(a.out, backend, a.stamps, rec.done(), rec);
    ::close(ack);
    return rec.done() ? 0 : 1;
}

/// Binds, then reads one message per datagram until every peer ends its run.
template <transport::datagram::DatagramChannel C>
int run_channel_receiver(const ReceiverArgs& a, std::string_view backend) {
    using transport::datagram::ChannelStatus;
    const int  ack      = udp_socket(nullptr, nullptr);
    const auto ack_base = to_sockaddr(a.ack);
    if (ack < 0 || !ack_base) {
        std::perror("ack socket");
        return 1;
    }
    C c;
    if (c.bind(a.target.ip, a.target.port) != ChannelStatus::Open) {
        std::fprintf(stderr, "%.*s: bind failed\n", static_cast<int>(backend.size()),
                     backend.data());
        return 1;
    }

    Recorder rec(ack, *ack_base, a.peers);
    rec.ready();
    // Datagrams from one receive share its user stamp, so a change of stamp ends a batch.
    std::int64_t batch_ts = 0;
    std::size_t  batch    = 0;
    while (!rec.done()) {
        rec.nudge();
        const auto d = c.read();
        if (!d.lent) {
            if (d.status != ChannelStatus::Open || rec.idle(a.idle_ns)) break;
            continue;
        }
        if (d.rx_ts.user != batch_ts) {
            if (batch) rec.on_read(batch);
            batch_ts = d.rx_ts.user;
            batch    = 0;
        }
        ++batch;
        const auto bytes = d.lent.bytes();
        if (bytes.size() < sizeof(MessageHeader)) {
            rec.bad_size();
            continue;
        }
        const MessageHeader h = read_header(bytes);
        if (h.size != bytes.size()) {
            rec.bad_size();
            continue;
        }
        rec.on_message(h, d.rx_ts, mono_ns());
    }
    if (batch) rec.on_read(batch);
    rec.lost(c.dropped(), c.truncated());
    write_report(a.out, backend, a.stamps, rec.done(), rec);
    ::close(ack);
    return rec.done() ? 0 : 1;
}

}  // namespace qp::network::rig
