#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "types.hpp"

namespace qp::wire {

// The on-disk record format for MarketEvent — the shared contract between
// FileRecorder (write side) and FileReplaySource (read side). Both call
// these same functions rather than maintaining two independent
// implementations that have to be kept in sync by discipline (see
// docs/decisions.md D12).
//
// Fixed layout, written in this order:
//   EventKind kind        (1 byte, EventKind's underlying type is uint8_t)
//   int64_t   ts
//   uint64_t  first_seq
//   uint64_t  seq
//   uint64_t  prev_seq
//   uint32_t  symbol
//   uint32_t  bid_count;  PriceLevel[bid_count]
//   uint32_t  ask_count;  PriceLevel[ask_count]
//   double    price
//   double    qty
//   Side      side        (1 byte, Side's underlying type is uint8_t)
//   double    funding_rate
//
// Every event carries every field regardless of kind (e.g. price/qty/side
// on a BookDiff go unused) — a fixed, branch-free layout in exchange for a
// few wasted bytes per record. Simpler than a tagged-union format; revisit
// only if size actually matters (it won't for BookDiff, the dominant record
// type — bids/asks dwarf the fixed portion).
//
// No endianness handling: this project only ever reads back what it itself
// wrote, on the same little-endian architecture family (x86_64/ARM64 dev
// and prod, per docs/environment.md) — cross-platform wire portability
// isn't a requirement here, so it isn't built.

namespace detail {

template <class T>
void append_pod(std::vector<std::byte>& out, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* bytes = reinterpret_cast<const std::byte*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

// Symmetric with read_fields: appends several fields in one call, purely
// for read/write to visibly mirror each other field-group-for-field-group.
template <class... Ts>
void append_fields(std::vector<std::byte>& out, const Ts&... values) {
    (append_pod(out, values), ...);
}

template <class T>
bool read_pod(std::span<const std::byte>& in, T& out_value) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (in.size() < sizeof(T)) return false;
    std::memcpy(&out_value, in.data(), sizeof(T));
    in = in.subspan(sizeof(T));
    return true;
}

// Reads several fixed-size fields as one bounds check instead of one per
// field — cuts a run of N "enough bytes left?" branches (each individually
// near-always true, but still a branch) down to 1 for that group. Fields
// are copied at their exact byte offsets via individual memcpys, so unlike
// a struct overlay this has no padding/alignment to reason about.
template <class... Ts>
bool read_fields(std::span<const std::byte>& in, Ts&... values) {
    static_assert((std::is_trivially_copyable_v<Ts> && ...));
    constexpr std::size_t kTotal = (sizeof(Ts) + ...);
    if (in.size() < kTotal) return false;
    std::size_t offset = 0;
    ((std::memcpy(&values, in.data() + offset, sizeof(Ts)), offset += sizeof(Ts)), ...);
    in = in.subspan(kTotal);
    return true;
}

// Bulk-reads `count` PriceLevels directly into `out` (already sized) with
// one memcpy rather than `count` individual read_pod calls — safe because
// PriceLevel is static_assert'd trivially copyable (types.hpp). Returns
// false, leaving `in` untouched, if there aren't enough bytes — this bound
// check runs *before* any allocation-sizing decision the caller makes, so a
// truncated or corrupted count can't drive an oversized resize().
inline bool read_levels(std::span<const std::byte>& in, std::vector<PriceLevel>& out,
                        std::uint32_t count) {
    const std::size_t bytes_needed = static_cast<std::size_t>(count) * sizeof(PriceLevel);
    if (in.size() < bytes_needed) return false;
    out.resize(count);
    std::memcpy(out.data(), in.data(), bytes_needed);
    in = in.subspan(bytes_needed);
    return true;
}

}  // namespace detail

inline void write_event(std::vector<std::byte>& out, const MarketEvent& event) {
    detail::append_fields(out, event.kind, event.ts, event.first_seq, event.seq, event.prev_seq,
                          event.symbol);

    detail::append_pod(out, static_cast<std::uint32_t>(event.bids.size()));
    if (!event.bids.empty()) {
        const auto* bytes = reinterpret_cast<const std::byte*>(event.bids.data());
        out.insert(out.end(), bytes, bytes + event.bids.size() * sizeof(PriceLevel));
    }

    detail::append_pod(out, static_cast<std::uint32_t>(event.asks.size()));
    if (!event.asks.empty()) {
        const auto* bytes = reinterpret_cast<const std::byte*>(event.asks.data());
        out.insert(out.end(), bytes, bytes + event.asks.size() * sizeof(PriceLevel));
    }

    detail::append_fields(out, event.price, event.qty, event.side, event.funding_rate);
}

// Returns nullopt if `in` doesn't hold one complete record — the deliberate
// crash-safety/truncated-tail contract (D12), not an error: a partial
// record at the end of a file (the last thing written before a crash) is
// simply not there yet, as far as a reader is concerned. `in` is only
// advanced when a full record was read; on nullopt it's untouched, so a
// caller can safely stop and treat whatever's left as unreadable rather
// than partially consuming a corrupt/incomplete tail.
inline std::optional<MarketEvent> read_event(std::span<const std::byte>& in) {
    std::span<const std::byte> cursor = in;

    MarketEvent event;
    if (!detail::read_fields(cursor, event.kind, event.ts, event.first_seq, event.seq,
                             event.prev_seq, event.symbol)) {
        return std::nullopt;
    }

    std::uint32_t bid_count;
    if (!detail::read_pod(cursor, bid_count)) return std::nullopt;
    if (!detail::read_levels(cursor, event.bids, bid_count)) return std::nullopt;

    std::uint32_t ask_count;
    if (!detail::read_pod(cursor, ask_count)) return std::nullopt;
    if (!detail::read_levels(cursor, event.asks, ask_count)) return std::nullopt;

    if (!detail::read_fields(cursor, event.price, event.qty, event.side, event.funding_rate)) {
        return std::nullopt;
    }

    in = cursor;  // commit: a full record was read
    return event;
}

}  // namespace qp::wire
