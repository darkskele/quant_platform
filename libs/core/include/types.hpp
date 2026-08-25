#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>

namespace qp {

using Timestamp = std::int64_t;   ///< Nanoseconds since epoch.
using Price     = double;         ///< TODO: fixed-point ticks for exactness — see D26.
using Qty       = double;         ///< Same exactness gap as Price (D26): step size, not tick size.
using SymbolId  = std::uint32_t;  ///< Index into venue symbol table.
using VenueId   = std::uint8_t;   ///< Which leg/venue produced this event — meaningless on its
                                  ///< own (like SymbolId), interpreted by whoever wired the
                                  ///< producing FanoutSink (D43). (SymbolId, VenueId) together
                                  ///< identify an instrument; SymbolId alone does not, since two
                                  ///< venues each intern "BTCUSDT" to their own SymbolId 0.

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
/// Field order is alignment-driven, not logical (D32): kind/side/symbol
/// grouped first — they're the only sub-8-byte fields — saves 16 bytes of
/// padding (128 -> 112) versus declaration order. wire.hpp encodes
/// field-by-field, not memcpy, so this doesn't touch the on-disk format.
struct MarketEvent {
    EventKind kind{};
    Side      side{};   ///< Trade only.
    VenueId   venue{};  ///< Stamped by FanoutSink::record<I>() (D43); 0 for single-venue paths
                        ///< (collector, FileRecorder) that never disambiguate.
    SymbolId      symbol{};
    Timestamp     ts{};         ///< Exchange/event time.
    std::uint64_t first_seq{};  ///< First seq in event (Binance's U); BookDiff only.
    std::uint64_t seq{};        ///< Final update id (Binance's u); gap detection/resync.
    std::uint64_t prev_seq{};   ///< Continues-from seq (pu); 0/unset if not applicable.

    // Trade:
    Price price{};
    Qty   qty{};

    // Funding: both come off Binance's markPriceUpdate stream/event together
    // (the same message carries both fields), not two separate events.
    Price mark_price{};  ///< Binance's mark price — funding settles against this, not last trade.
    double funding_rate{};

    // BookDiff:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

static_assert(std::is_standard_layout_v<MarketEvent>);
// Deliberately NOT trivially-copyable: bids/asks own std::vector. Queue/wire
// code must move or explicitly serialize it, never memcpy it. If this ever
// flips to true, something's wrong (or the container choice changed on
// purpose — update this assert either way, don't just delete it).
static_assert(!std::is_trivially_copyable_v<MarketEvent>);

using OrderId  = std::uint64_t;  ///< Caller-assigned; unique per submitted Order.
using Notional = double;         ///< Quote-currency amount (fees, PnL).

/// A strategy's desired end-state for one symbol — a target position, not
/// a delta or a venue order ("be +2 BTC", not "buy 2 BTC"). RiskGate turns
/// this into concrete Order(s), computing the delta itself against current
/// StateView. No ts field — same as Order, timestamps are call-site
/// parameters where actually consumed, not struct fields.
struct Intent {
    SymbolId symbol{};
    Qty      target_position{};  ///< Signed: positive = net long, negative = net short.
};

/// A request to trade. Market order only — no price/type field yet; earns
/// its place when limit orders do.
struct Order {
    OrderId  id{};
    SymbolId symbol{};
    Side     side{};
    Qty      qty{};
};

/// What happened to a submitted Order: filled. Field order is
/// alignment-driven (D32): symbol/side grouped right after order_id saves
/// 8 bytes of padding (56 -> 48) versus declaration order.
struct Fill {
    OrderId   order_id{};
    SymbolId  symbol{};
    Side      side{};
    Timestamp ts{};
    Price     price{};
    Qty       qty{};  ///< == Order::qty always, for now — no partials.
    Notional  fee{};
};

static_assert(sizeof(Fill) == 48, "unexpected padding/size regression");

/// Reasons grow as real ones appear (margin, invalid qty, exchange
/// downtime) — NoPriceAvailable is SimExecution's only one today.
enum class RejectReason : std::uint8_t { NoPriceAvailable };

/// What happened to a submitted Order: didn't. Field order is
/// alignment-driven (D32): symbol/reason grouped right after order_id
/// saves 8 bytes of padding (32 -> 24) versus declaration order.
struct Reject {
    OrderId      order_id{};
    SymbolId     symbol{};
    RejectReason reason{};
    Timestamp    ts{};
};

static_assert(sizeof(Reject) == 24, "unexpected padding/size regression");

}  // namespace qp
