#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "histogram.hpp"
#include "traffic.hpp"

namespace qp::network::rig {

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

/// What a sender did, for its report.
struct SendStats {
    std::int64_t  elapsed_ns  = 0;
    std::uint64_t behind      = 0;
    std::int64_t  worst_late  = 0;
    std::uint64_t sends       = 0;
    std::uint64_t missed_acks = 0;
    std::uint64_t send_errors = 0;
    std::uint64_t full_waits  = 0;  ///< writes that found no room and waited
};

/// The sender's report as JSON, to path or stdout. write_call is how long each send held the
/// caller, when the sender times it.
inline void write_send_report(const std::string& path, std::string_view proto, const Traffic& t,
                              const SendStats& st, const Histogram* write_call) {
    std::FILE* f = path.empty() ? stdout : std::fopen(path.c_str(), "w");
    if (!f) {
        std::perror(path.c_str());
        return;
    }
    std::fprintf(f,
                 "{\"proto\": \"%.*s\", \"mode\": \"%s\", \"sizes\": \"%s\", \"rate\": %llu, "
                 "\"burst\": %llu, \"batch\": %llu, \"sends\": %llu, \"seed\": %llu, \"count\": "
                 "%llu, \"warmup\": %llu, \"elapsed_ns\": %lld, \"behind\": %llu, "
                 "\"worst_late_ns\": %lld, \"missed_acks\": %llu, \"send_errors\": %llu, "
                 "\"full_waits\": %llu",
                 static_cast<int>(proto.size()), proto.data(), t.mode_name.c_str(),
                 t.sizes_text.c_str(), static_cast<unsigned long long>(t.rate),
                 static_cast<unsigned long long>(t.burst), static_cast<unsigned long long>(t.batch),
                 static_cast<unsigned long long>(st.sends), static_cast<unsigned long long>(t.seed),
                 static_cast<unsigned long long>(t.count),
                 static_cast<unsigned long long>(t.warmup), static_cast<long long>(st.elapsed_ns),
                 static_cast<unsigned long long>(st.behind), static_cast<long long>(st.worst_late),
                 static_cast<unsigned long long>(st.missed_acks),
                 static_cast<unsigned long long>(st.send_errors),
                 static_cast<unsigned long long>(st.full_waits));
    if (write_call) {
        std::fprintf(f, ",\n \"write_call\": ");
        write_histogram(f, *write_call);
    }
    std::fprintf(f, "}\n");
    if (f != stdout) std::fclose(f);
}

}  // namespace qp::network::rig
