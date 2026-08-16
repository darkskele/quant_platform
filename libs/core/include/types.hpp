#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>

namespace qp {

using Timestamp = std::int64_t;  ///< Nanoseconds since epoch.
using Price     = double;        ///< TODO: consider fixed-point ticks for exactness.
using Qty       = double;
using SymbolId  = std::uint32_t;  ///< Index into venue symbol table.

enum class Side : std::uint8_t { Buy, Sell };

struct PriceLevel {
    Price price{};
    Qty   qty{};  ///< 0 qty in a diff = level removed.
};

static_assert(
    std::is_trivially_copyable_v<PriceLevel>);  // safe to memcpy — wire format relies on this
static_assert(sizeof(PriceLevel) == 16, "unexpected padding/size regression");

/// BookSnapshot: a full-book-replace anchor (a resync's REST snapshot,
/// forwarded as an event instead of consumed-and-discarded internally) —
/// carries bids/asks the same as BookDiff, but means "clear the book, then
/// apply these" rather than "apply these on top of what's there." Always
/// the first event after a resync, immediately preceding the replayed
/// diffs that are now guaranteed to apply cleanly on top of it. Without
/// this, a recording has no independent baseline to reconstruct from — see
/// docs/decisions.md.
enum class EventKind : std::uint8_t { BookDiff, Trade, Funding, BookSnapshot };

/// The lingua franca. Plain data, no behavior, no venue-specifics.
/// Kept as one struct (not a variant) so it serializes trivially to `wire`.
struct MarketEvent {
    EventKind     kind{};
    Timestamp     ts{};         ///< Exchange/event time.
    std::uint64_t first_seq{};  ///< First seq in event (Binance's U); BookDiff only.
    std::uint64_t seq{};        ///< Final update id (Binance's u); gap detection/resync.
    std::uint64_t prev_seq{};   ///< Continues-from seq (pu); 0/unset if not applicable.
    SymbolId      symbol{};

    // BookDiff:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;

    // Trade:
    Price price{};
    Qty   qty{};
    Side  side{};

    // Funding:
    double funding_rate{};
};

static_assert(std::is_standard_layout_v<MarketEvent>);
// Deliberately NOT trivially-copyable: bids/asks own std::vector. Queue/wire
// code must move or explicitly serialize it, never memcpy it. If this ever
// flips to true, something's wrong (or the container choice changed on
// purpose — update this assert either way, don't just delete it).
static_assert(!std::is_trivially_copyable_v<MarketEvent>);

}  // namespace qp
