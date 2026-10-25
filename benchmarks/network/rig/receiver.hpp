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

#include "byte_stream.hpp"
#include "datagram_channel.hpp"
#include "endpoint.hpp"
#include "histogram.hpp"
#include "message.hpp"
#include "rx_times.hpp"
#include "stream_framer.hpp"

namespace qp::network::rig {

/// Per stage latencies and correctness counts for one receiver run.
class Recorder {
   public:
    explicit Recorder(int ack_fd) noexcept : ack_fd_(ack_fd), last_rx_(mono_ns()) {}

    /// Tells the peer to start. Repeated by idle() until the first message, in case the peer
    /// was not listening yet.
    void ready() noexcept {
        ack(READY);
        last_ready_ = mono_ns();
    }

    /// app is when the message was parsed.
    void on_message(const MessageHeader& h, const transport::RxTimes& t,
                    std::int64_t app) noexcept {
        if (h.flags & END) {
            done_ = true;
            return;
        }
        track(h.seq);
        started_ = true;
        last_rx_ = t.user;
        if (!(h.flags & WARMUP)) {
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
        if (h.flags & ACK) ack(h.seq);
    }

    /// Messages one stream read completed.
    void on_read(std::size_t messages) noexcept {
        per_read_.record(static_cast<std::int64_t>(messages));
    }

    void bad_framing() noexcept { ++bad_framing_; }

    void bad_size() noexcept { ++bad_size_; }

    void lost(std::uint64_t dropped, std::uint64_t truncated) noexcept {
        dropped_   = dropped;
        truncated_ = truncated;
    }

    bool done() const noexcept { return done_; }

    /// True once nothing has arrived for limit_ns. Reads the clock once in 1024 calls.
    bool idle(std::int64_t limit_ns) noexcept {
        if (++idle_polls_ % 1024 != 0) return false;
        const std::int64_t now = mono_ns();
        if (!started_ && now - last_ready_ > 100'000'000) ready();
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
        f("dropped", dropped_);
        f("truncated", truncated_);
    }

   private:
    void ack(std::uint64_t seq) const noexcept { ::send(ack_fd_, &seq, sizeof seq, 0); }

    void track(std::uint64_t seq) noexcept {
        if (seq == next_) {
            ++next_;
        } else if (seq > next_) {
            gaps_ += seq - next_;
            next_ = seq + 1;
        } else {
            ++late_;
        }
    }

    int           ack_fd_;
    std::int64_t  last_rx_;
    std::uint64_t idle_polls_ = 0;
    std::int64_t  last_ready_ = 0;
    bool          started_    = false;
    bool          done_       = false;
    std::uint64_t next_       = 0;

    Histogram send_to_user_, send_to_app_, user_to_app_, send_to_kernel_, kernel_to_user_,
        per_read_;

    std::uint64_t messages_ = 0, warmup_ = 0, gaps_ = 0, late_ = 0, bad_framing_ = 0, bad_size_ = 0,
                  dropped_ = 0, truncated_ = 0;
};

struct ReceiverArgs {
    Endpoint     target;  ///< The peer to connect to on a stream, the address to bind on a channel.
    Endpoint     ack;     ///< The peer's ack socket.
    std::string  stamps = "none";
    std::string  out;
    std::int64_t idle_ns = 2'000'000'000;
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
                 "usage: %s --target ip:port --ack ip:port [--stamps none|kernel] [--out file] "
                 "[--idle-ms n]\n",
                 argv[0]);
    return std::nullopt;
}

template <int PrecisionBits, int RangeBits>
void write_histogram(std::FILE* f, const LogLinearHistogram<PrecisionBits, RangeBits>& h) {
    std::fprintf(f, "{\"precision_bits\": %d, \"range_bits\": %d, ", PrecisionBits, RangeBits);
    std::fprintf(
        f,
        "\"count\": %llu, \"negative\": %llu, \"min\": %lld, \"max\": %lld, \"mean\": "
        "%.1f, \"p50\": %lld, \"p90\": %lld, \"p99\": %lld, \"p99.9\": %lld, \"p99.99\": "
        "%lld, \"buckets\": [",
        static_cast<unsigned long long>(h.count()), static_cast<unsigned long long>(h.negative()),
        static_cast<long long>(h.min()), static_cast<long long>(h.max()), h.mean(),
        static_cast<long long>(h.percentile(50)), static_cast<long long>(h.percentile(90)),
        static_cast<long long>(h.percentile(99)), static_cast<long long>(h.percentile(99.9)),
        static_cast<long long>(h.percentile(99.99)));
    bool first = true;
    h.each([&](std::int64_t lo, std::int64_t hi, std::uint64_t n) {
        std::fprintf(f, "%s[%lld, %lld, %llu]", first ? "" : ", ", static_cast<long long>(lo),
                     static_cast<long long>(hi), static_cast<unsigned long long>(n));
        first = false;
    });
    std::fprintf(f, "]}");
}

/// The run as JSON, to path or to stdout when path is empty. Values are nanoseconds.
inline void write_report(const std::string& path, std::string_view backend, std::string_view stamps,
                         bool completed, const Recorder& rec) {
    std::FILE* f = path.empty() ? stdout : std::fopen(path.c_str(), "w");
    if (!f) {
        std::perror(path.c_str());
        return;
    }
    std::fprintf(f, "{\"backend\": \"%.*s\", \"stamps\": \"%.*s\", \"completed\": %s,\n",
                 static_cast<int>(backend.size()), backend.data(), static_cast<int>(stamps.size()),
                 stamps.data(), completed ? "true" : "false");
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
    const int ack = udp_socket(nullptr, &a.ack);
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

    Recorder rec(ack);
    rec.ready();
    StreamFramer framer;
    while (!rec.done()) {
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

/// Binds, then reads one message per datagram until the peer ends the run.
template <transport::datagram::DatagramChannel C>
int run_channel_receiver(const ReceiverArgs& a, std::string_view backend) {
    using transport::datagram::ChannelStatus;
    const int ack = udp_socket(nullptr, &a.ack);
    if (ack < 0) {
        std::perror("ack socket");
        return 1;
    }
    C c;
    if (c.bind(a.target.ip, a.target.port) != ChannelStatus::Open) {
        std::fprintf(stderr, "%.*s: bind failed\n", static_cast<int>(backend.size()),
                     backend.data());
        return 1;
    }

    Recorder rec(ack);
    rec.ready();
    while (!rec.done()) {
        const auto d = c.read();
        if (!d.lent) {
            if (d.status != ChannelStatus::Open || rec.idle(a.idle_ns)) break;
            continue;
        }
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
    rec.lost(c.dropped(), c.truncated());
    write_report(a.out, backend, a.stamps, rec.done(), rec);
    ::close(ack);
    return rec.done() ? 0 : 1;
}

}  // namespace qp::network::rig
