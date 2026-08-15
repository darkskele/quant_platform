#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>

namespace qp {

using Timestamp = std::int64_t;  // nanoseconds since epoch
using Price     = double;        // TODO: consider fixed-point ticks for exactness
using Qty       = double;
using SymbolId  = std::uint32_t; // index into venue symbol table

enum class Side : std::uint8_t { Buy, Sell };

struct PriceLevel {
    Price price{};
    Qty   qty{};  // 0 qty in a diff = level removed
};
static_assert(std::is_trivially_copyable_v<PriceLevel>);  // safe to memcpy — wire format relies on this
static_assert(sizeof(PriceLevel) == 16, "unexpected padding/size regression");

enum class EventKind : std::uint8_t { BookDiff, Trade, Funding };

// The lingua franca. Plain data, no behavior, no venue-specifics.
// Kept as one struct (not a variant) so it serializes trivially to `wire`.
struct MarketEvent {
    EventKind     kind{};
    Timestamp     ts{};        // exchange/event time
    std::uint64_t first_seq{}; // first sequence number in this event (Binance's U); BookDiff only
    std::uint64_t seq{};       // sequence number (final update id, Binance's u), for gap detection / resync
    std::uint64_t prev_seq{};  // sequence this event continues from (pu); 0/unset if not applicable
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
