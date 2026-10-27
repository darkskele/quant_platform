#pragma once
#include <sys/socket.h>
#include <sys/time.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "message.hpp"

namespace qp::network::rig {

/// How sends are spaced. ack waits for each message's ack, flood never waits.
enum class Mode { Ack, Fixed, Poisson, Burst, Flood };

struct SizeWeight {
    std::uint32_t size   = 0;
    double        weight = 1;
};

/// What a sender sends, and how fast.
struct Traffic {
    Mode                    mode      = Mode::Ack;
    std::string             mode_name = "ack";
    std::vector<SizeWeight> sizes{{64, 1}};
    std::string             sizes_text = "64";
    std::uint64_t           count      = 100'000;
    std::uint64_t           warmup     = 10'000;
    std::uint64_t           rate   = 10'000;  ///< messages per second, bursts per second for burst
    std::uint64_t           burst  = 8;
    std::uint64_t           batch  = 1;  ///< most messages one send takes
    std::uint64_t           seed   = 1;
    int                     ack_ms = 100;
};

/// A send this late against its schedule counts as behind.
inline constexpr std::int64_t LATE_NS = 10'000;

template <class T>
bool parse_number(std::string_view text, T& out) {
    const auto [end, err] = std::from_chars(text.data(), text.data() + text.size(), out);
    return err == std::errc{} && end == text.data() + text.size();
}

/// size or size:weight, comma separated.
inline bool parse_sizes(std::string_view text, std::vector<SizeWeight>& out) {
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

/// Reads one traffic argument. False when key is not one, otherwise ok says whether value was
/// good.
inline bool parse_traffic(std::string_view key, std::string_view value, Traffic& t, bool& ok) {
    if (key == "--mode") {
        const std::pair<std::string_view, Mode> modes[] = {{"ack", Mode::Ack},
                                                           {"fixed", Mode::Fixed},
                                                           {"poisson", Mode::Poisson},
                                                           {"burst", Mode::Burst},
                                                           {"flood", Mode::Flood}};
        const auto m = std::ranges::find(modes, value, &std::pair<std::string_view, Mode>::first);
        ok           = m != std::end(modes);
        if (ok) t.mode = m->second;
        t.mode_name = value;
    } else if (key == "--sizes") {
        ok           = parse_sizes(value, t.sizes);
        t.sizes_text = value;
    } else if (key == "--count") {
        ok = parse_number(value, t.count);
    } else if (key == "--warmup") {
        ok = parse_number(value, t.warmup);
    } else if (key == "--rate") {
        ok = parse_number(value, t.rate) && t.rate > 0;
    } else if (key == "--burst") {
        ok = parse_number(value, t.burst) && t.burst > 0;
    } else if (key == "--batch") {
        ok = parse_number(value, t.batch) && t.batch > 0 && t.batch <= 1024;
    } else if (key == "--seed") {
        ok = parse_number(value, t.seed);
    } else if (key == "--ack-ms") {
        ok = parse_number(value, t.ack_ms) && t.ack_ms > 0;
    } else {
        return false;
    }
    return true;
}

inline constexpr const char* TRAFFIC_USAGE =
    "  [--mode ack|fixed|poisson|burst|flood] [--rate per_second] [--burst n]\n"
    "  [--sizes size[:weight],...] [--batch n] [--count n] [--warmup n] [--seed n] [--ack-ms n]\n";

/// Every message's size and send time from the start, drawn once from the seed so each run sends
/// the same traffic.
struct Schedule {
    std::vector<std::uint32_t> sizes;
    std::vector<std::int64_t>  due_ns;
};

inline Schedule make_schedule(const Traffic& t) {
    const std::uint64_t total = t.warmup + t.count;
    Schedule            s{std::vector<std::uint32_t>(total), std::vector<std::int64_t>(total, 0)};
    std::mt19937_64     rng(t.seed);

    std::vector<double> weights;
    for (const auto& sw : t.sizes) weights.push_back(sw.weight);
    std::discrete_distribution<std::size_t> pick(weights.begin(), weights.end());
    for (auto& size : s.sizes) size = t.sizes[pick(rng)].size;

    const double                          gap_ns = 1e9 / static_cast<double>(t.rate);
    std::exponential_distribution<double> poisson_gap(1 / gap_ns);
    double                                at = 0;
    for (std::uint64_t i = 0; i < total; ++i) {
        switch (t.mode) {
            case Mode::Fixed:
                at = static_cast<double>(i) * gap_ns;
                break;
            case Mode::Poisson:
                at += i ? poisson_gap(rng) : 0;
                break;
            case Mode::Burst:
                at = static_cast<double>(i / t.burst) * gap_ns;
                break;
            case Mode::Ack:
            case Mode::Flood:
                break;
        }
        s.due_ns[i] = static_cast<std::int64_t>(at);
    }
    return s;
}

/// Spaces sends to a schedule and counts how late they went.
class Pacer {
   public:
    Pacer(const Traffic& t, const Schedule& s)
        : t_(t),
          s_(s),
          paced_(t.mode == Mode::Fixed || t.mode == Mode::Poisson || t.mode == Mode::Burst) {}

    void start() noexcept { start_ = mono_ns(); }

    /// Spins until message i is due.
    void wait(std::uint64_t i) noexcept {
        if (!paced_) return;
        const std::int64_t due = start_ + s_.due_ns[i];
        std::int64_t       now = mono_ns();
        while (now < due) now = mono_ns();
        // A burst is due at its first message. The rest follow it back to back.
        if (t_.mode != Mode::Burst || i % t_.burst == 0) {
            worst_late_ = std::max(worst_late_, now - due);
            if (now - due > LATE_NS) ++behind_;
        }
    }

    /// How many messages from i go in one send at now, up to batch and only those already due.
    /// Each waits for its own ack in ack mode, so never batches.
    std::uint64_t batch_from(std::uint64_t i, std::int64_t now) const noexcept {
        std::uint64_t n = 1;
        while (n < t_.batch && i + n < s_.sizes.size() && t_.mode != Mode::Ack &&
               (!paced_ || start_ + s_.due_ns[i + n] <= now))
            ++n;
        return n;
    }

    std::int64_t elapsed() const noexcept { return mono_ns() - start_; }

    std::uint64_t behind() const noexcept { return behind_; }

    std::int64_t worst_late() const noexcept { return worst_late_; }

   private:
    const Traffic&  t_;
    const Schedule& s_;
    bool            paced_;
    std::int64_t    start_      = 0;
    std::uint64_t   behind_     = 0;
    std::int64_t    worst_late_ = 0;
};

inline void set_timeout(int fd, int ms) {
    const timeval tv{ms / 1000, (ms % 1000) * 1000};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

/// Waits for the ack carrying seq, skipping older ones. False on a timeout.
inline bool await_ack(int ack, std::uint64_t seq) {
    for (;;) {
        std::uint64_t got = 0;
        if (::recv(ack, &got, sizeof got, 0) != sizeof got) return false;
        if (got == seq) return true;
    }
}

}  // namespace qp::network::rig
