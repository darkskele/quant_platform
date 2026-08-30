#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "types.hpp"

namespace qp::data_source::wire {

// The on-disk/on-ring record format for MarketEvent — the shared contract
// between whatever writes it (FileReplaySource's future historical-data
// converter, FanoutSink's ring transport) and whatever reads it back. Both
// call these same functions rather than maintaining independent
// implementations kept in sync by discipline (D12).
//
// Per-kind, not one fixed layout every kind pays for regardless of use
// (the old design — see git history): a shared 15-byte header
// (kind, venue, symbol, ts), followed by exactly that kind's own fields —
// nothing else. A Trade record is small; a BookDiff record carries
// whatever its actual level count is, not a fixed reservation for the
// worst case.
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

// Bulk-writes a vector<PriceLevel> as a count followed by its raw bytes —
// safe because PriceLevel is static_assert'd trivially copyable
// (types.hpp).
inline void append_levels(std::vector<std::byte>& out, const std::vector<PriceLevel>& levels) {
    append_pod(out, static_cast<std::uint32_t>(levels.size()));
    if (!levels.empty()) {
        const auto* bytes = reinterpret_cast<const std::byte*>(levels.data());
        out.insert(out.end(), bytes, bytes + levels.size() * sizeof(PriceLevel));
    }
}

// Bulk-reads `count` PriceLevels directly into `out` (already sized) with
// one memcpy rather than `count` individual read_pod calls. Returns false,
// leaving `in` untouched, if there aren't enough bytes — this bound check
// runs *before* any allocation-sizing decision the caller makes, so a
// truncated or corrupted count can't drive an oversized resize().
inline bool read_levels(std::span<const std::byte>& in, std::vector<PriceLevel>& out,
                        std::uint32_t count) {
    const std::size_t bytes_needed = static_cast<std::size_t>(count) * sizeof(PriceLevel);
    if (in.size() < bytes_needed) return false;
    out.resize(count);
    // count == 0 leaves out.data()/in.data() potentially null — memcpy's
    // arguments are declared never-null even at size 0, so skip the call
    // entirely rather than pass a null pointer into it (UB, UBSan-caught).
    if (count > 0) std::memcpy(out.data(), in.data(), bytes_needed);
    in = in.subspan(bytes_needed);
    return true;
}

inline bool read_levels_pair(std::span<const std::byte>& in, std::vector<PriceLevel>& bids,
                             std::vector<PriceLevel>& asks) {
    std::uint32_t bid_count;
    if (!read_pod(in, bid_count)) return false;
    if (!read_levels(in, bids, bid_count)) return false;
    std::uint32_t ask_count;
    if (!read_pod(in, ask_count)) return false;
    return read_levels(in, asks, ask_count);
}

}  // namespace detail

inline void write_event(std::vector<std::byte>& out, const MarketEvent& event) {
    std::visit(
        [&out](const auto& e) {
            using E = std::decay_t<decltype(e)>;
            detail::append_fields(out, e.kind, e.venue, e.symbol, e.ts);

            if constexpr (std::is_same_v<E, TradeEvent>) {
                detail::append_fields(out, e.side, e.price, e.qty);
            } else if constexpr (std::is_same_v<E, FundingEvent>) {
                detail::append_fields(out, e.mark_price, e.funding_rate);
            } else if constexpr (std::is_same_v<E, KlineEvent>) {
                detail::append_fields(out, e.close_time, e.open, e.high, e.low, e.close, e.volume);
            } else if constexpr (std::is_same_v<E, BookDiffEvent>) {
                detail::append_fields(out, e.first_seq, e.seq, e.prev_seq);
                static const BookLevels kEmpty{};
                const BookLevels&       levels = e.levels ? *e.levels : kEmpty;
                detail::append_levels(out, levels.bids);
                detail::append_levels(out, levels.asks);
            } else if constexpr (std::is_same_v<E, BookSnapshotEvent>) {
                static const BookLevels kEmpty{};
                const BookLevels&       levels = e.levels ? *e.levels : kEmpty;
                detail::append_levels(out, levels.bids);
                detail::append_levels(out, levels.asks);
            }
        },
        event);
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

    EventKind kind;
    VenueId   venue;
    SymbolId  symbol;
    Timestamp ts;
    if (!detail::read_fields(cursor, kind, venue, symbol, ts)) return std::nullopt;

    switch (kind) {
        case EventKind::Trade: {
            Side  side;
            Price price;
            Qty   qty;
            if (!detail::read_fields(cursor, side, price, qty)) return std::nullopt;
            TradeEvent e;
            e.venue  = venue;
            e.symbol = symbol;
            e.ts     = ts;
            e.side   = side;
            e.price  = price;
            e.qty    = qty;
            in       = cursor;
            return e;
        }
        case EventKind::Funding: {
            Price  mark_price;
            double funding_rate;
            if (!detail::read_fields(cursor, mark_price, funding_rate)) return std::nullopt;
            FundingEvent e;
            e.venue        = venue;
            e.symbol       = symbol;
            e.ts           = ts;
            e.mark_price   = mark_price;
            e.funding_rate = funding_rate;
            in             = cursor;
            return e;
        }
        case EventKind::Kline: {
            Timestamp close_time;
            Price     open, high, low, close;
            Qty       volume;
            if (!detail::read_fields(cursor, close_time, open, high, low, close, volume)) {
                return std::nullopt;
            }
            KlineEvent e;
            e.venue      = venue;
            e.symbol     = symbol;
            e.ts         = ts;
            e.close_time = close_time;
            e.open       = open;
            e.high       = high;
            e.low        = low;
            e.close      = close;
            e.volume     = volume;
            in           = cursor;
            return e;
        }
        case EventKind::BookDiff: {
            std::uint64_t first_seq, seq, prev_seq;
            if (!detail::read_fields(cursor, first_seq, seq, prev_seq)) return std::nullopt;
            auto levels = std::make_shared<BookLevels>();
            if (!detail::read_levels_pair(cursor, levels->bids, levels->asks)) return std::nullopt;
            BookDiffEvent e;
            e.venue     = venue;
            e.symbol    = symbol;
            e.ts        = ts;
            e.first_seq = first_seq;
            e.seq       = seq;
            e.prev_seq  = prev_seq;
            e.levels    = std::move(levels);
            in          = cursor;
            return e;
        }
        case EventKind::BookSnapshot: {
            auto levels = std::make_shared<BookLevels>();
            if (!detail::read_levels_pair(cursor, levels->bids, levels->asks)) return std::nullopt;
            BookSnapshotEvent e;
            e.venue  = venue;
            e.symbol = symbol;
            e.ts     = ts;
            e.levels = std::move(levels);
            in       = cursor;
            return e;
        }
    }
    return std::nullopt;
}

}  // namespace qp::data_source::wire
