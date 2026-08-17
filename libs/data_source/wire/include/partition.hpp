#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace qp::wire {

// A day, UTC, as a count of days since the Unix epoch — the only question
// this needs to answer is "same day as the currently-open partition, or
// not," a single integer comparison. Keyed off an event's own exchange
// timestamp (MarketEvent::ts), not wall-clock write time: that's what makes
// a partition's contents match what a later reader expects ("this file is
// Aug 15's data") regardless of when the collector happened to write each
// byte, and it means rotation only ever happens because an event for a new
// day actually arrived — never speculatively near a UTC midnight boundary
// on a quiet connection.
using DayKey = std::int64_t;

inline DayKey day_key_for(Timestamp ts_ns) {
    using namespace std::chrono;
    return floor<days>(nanoseconds(ts_ns)).count();
}

// "YYYY-MM-DD", for building a partition's filename. Only called when a
// rotation actually happens (at most once per symbol per day) — not on
// every event — so it doesn't need to be fast, just correct.
inline std::string format_day(DayKey day) {
    using namespace std::chrono;
    year_month_day ymd{sys_days{days{day}}};
    return std::format("{:%Y-%m-%d}", ymd);
}

// Pure decision, extracted so it's directly testable rather than only
// provable by inspecting FileRecorder's actual file I/O (same reasoning as
// find_resync_point/ResyncCoordinator): does the currently-open partition
// (if any) need to be closed and a new one opened for `event_day`?
inline bool needs_rotation(std::optional<DayKey> currently_open, DayKey event_day) {
    return !currently_open || *currently_open != event_day;
}

// Filename for the `seq`-th segment of `symbol_dir`'s `day` partition — the
// on-disk naming convention (`{date}.{seq:03d}.bin.zst`) shared by the
// write side (FileRecorder, picking the next free seq to open) and the
// read side (FileReplaySource, enumerating existing segments), so the
// format string exists in exactly one place rather than two that could
// drift apart.
inline std::filesystem::path segment_path(const std::filesystem::path& symbol_dir, DayKey day,
                                          int seq) {
    return symbol_dir / std::format("{}.{:03d}.bin.zst", format_day(day), seq);
}

// Every existing segment for `symbol_dir`'s `day`, in write order (seq 0,
// 1, 2, ...). Stops at the first missing seq — safe because FileRecorder
// only ever opens the next integer, never a sparse sequence, so a gap means
// "no more segments," not "a hole to skip past."
inline std::vector<std::filesystem::path> list_segments(const std::filesystem::path& symbol_dir,
                                                         DayKey day) {
    std::vector<std::filesystem::path> segments;
    for (int seq = 0;; ++seq) {
        auto path = segment_path(symbol_dir, day, seq);
        if (!std::filesystem::exists(path)) break;
        segments.push_back(std::move(path));
    }
    return segments;
}

}  // namespace qp::wire
