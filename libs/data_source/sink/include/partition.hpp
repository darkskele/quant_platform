#pragma once
#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

#include "types.hpp"

namespace qp::sink {

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

}  // namespace qp::sink
