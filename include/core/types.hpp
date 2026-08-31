#pragma once
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>
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
/// docs/decisions.md. Kline: an OHLCV bar — has a genuine live analog
/// (Binance's <symbol>@kline_<interval> stream), so it's a real event
/// kind, not squeezed into Trade at close price (D-TODO: log this as a
/// decision — synthesizing a Trade wouldn't match what live actually
/// sends, breaking the one-code-path-for-backtest-and-live principle
/// worse than adding a kind does).
enum class EventKind : std::uint8_t {
    BookDiff,
    Trade,
    Funding,
    BookSnapshot,
    Kline,
    MarkPriceKline
};

/// One flat struct per event kind, not one struct with every kind's fields
/// (the old design — every event paid for every other kind's unused
/// fields, and it only got worse as kinds were added, see git history).
/// Every kind's first four members are kind/venue/symbol/ts, in that
/// order — not enforced by the type system (std::variant's storage layout
/// is unspecified, unlike a raw union's blessed "common initial sequence"
/// rule — reinterpret-casting across alternatives isn't safe here), but
/// kept consistent by convention so header_of() below reads the same way
/// regardless of which kind it's looking at.
///
/// Trade/Funding/Kline/MarkPriceKline are plain data, no heap members,
/// trivially copyable — cheap to move or copy regardless of consumer
/// count.
/// BookDiff/BookSnapshot hold their levels behind a shared_ptr instead of
/// an inline vector — deliberately: those two are the only variable-length
/// kinds (an inline vector would make every kind in the variant pay for
/// the widest possible book-diff), and the shared_ptr lets N ring
/// consumers share one already-parsed BookLevels for free (a plain copy
/// on every consumer's pop would deep-copy the vector N times — see
/// docs/decisions.md on why this is the one deliberate exception to
/// "every event kind is a flat value type").
struct TradeEvent {
    EventKind kind = EventKind::Trade;
    VenueId   venue{};
    SymbolId  symbol{};
    Timestamp ts{};
    Side      side{};
    Price     price{};
    Qty       qty{};
};

static_assert(std::is_trivially_copyable_v<TradeEvent>);

/// No mark_price field here (an earlier version of this struct had one) —
/// verified against real historical data (data.binance.vision's
/// fundingRate CSVs): the two are published as genuinely independent
/// datasets, not bundled together the way the live REST shape suggested.
/// Settlement still needs a mark price, but it comes from whatever
/// MarkPriceKlineEvent was most recently seen (Portfolio tracks it), not
/// from this struct — see MarkPriceKlineEvent's own comment for why that's
/// not just relocation but the more correct model.
struct FundingEvent {
    EventKind kind = EventKind::Funding;
    VenueId   venue{};
    SymbolId  symbol{};
    Timestamp ts{};
    double    funding_rate{};
};

static_assert(std::is_trivially_copyable_v<FundingEvent>);

struct KlineEvent {
    EventKind kind = EventKind::Kline;
    VenueId   venue{};
    SymbolId  symbol{};
    Timestamp ts{};  ///< Bar open time.
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
    Qty       volume{};
};

static_assert(std::is_trivially_copyable_v<KlineEvent>);

/// Binance's own computed fair-value price for the contract — a blend of
/// spot index price and a smoothed futures premium, published as its own
/// independent stream specifically so funding settlement can't be gamed
/// by spoofing the last trade price right at settlement time. Own event
/// kind, not a flag on KlineEvent (same reasoning as keeping
/// BookDiffEvent/BookSnapshotEvent separate despite the field overlap):
/// a flag a consumer has to remember to check is exactly the kind of thing
/// that gets forgotten somewhere; a distinct variant alternative makes
/// std::visit enforce the distinction instead. Same bar shape as
/// KlineEvent (Binance publishes it that way) minus volume — mark price
/// isn't a traded quantity, so every volume-ish column in the real data is
/// always zero, not worth carrying.
struct MarkPriceKlineEvent {
    EventKind kind = EventKind::MarkPriceKline;
    VenueId   venue{};
    SymbolId  symbol{};
    Timestamp ts{};  ///< Bar open time.
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
};

static_assert(std::is_trivially_copyable_v<MarkPriceKlineEvent>);

/// BookDiff/BookSnapshot's actual levels — see the class comment above for
/// why this sits behind a shared_ptr in both event structs instead of an
/// inline vector.
struct BookLevels {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

struct BookDiffEvent {
    EventKind                         kind = EventKind::BookDiff;
    VenueId                           venue{};
    SymbolId                          symbol{};
    Timestamp                         ts{};
    std::uint64_t                     first_seq{};  ///< First seq in event (Binance's U).
    std::uint64_t                     seq{};        ///< Final update id (Binance's u).
    std::uint64_t                     prev_seq{};   ///< Continues-from seq (pu); 0 if n/a.
    std::shared_ptr<const BookLevels> levels;
};

struct BookSnapshotEvent {
    EventKind                         kind = EventKind::BookSnapshot;
    VenueId                           venue{};
    SymbolId                          symbol{};
    Timestamp                         ts{};
    std::shared_ptr<const BookLevels> levels;
};

/// The lingua franca — every event this system moves around, one of six
/// kinds. Kept as the name `MarketEvent` (not renamed) since every
/// consumer already reasons about "the event stream" under that name;
/// what changed is that it's a variant now, not a flat struct.
using MarketEvent = std::variant<TradeEvent, FundingEvent, KlineEvent, MarkPriceKlineEvent,
                                 BookDiffEvent, BookSnapshotEvent>;

/// Just the fields every kind shares — kind/venue/symbol/ts — read via
/// std::visit (a jump table, not a decode) since std::variant gives no
/// safe way to peek a common prefix across alternatives the way a raw
/// union would. This is what a multi-ring merge (e.g.
/// BacktestInProcessTransport, ordering by ts) needs without caring which
/// kind it's looking at.
struct EventHeader {
    EventKind kind{};
    VenueId   venue{};
    SymbolId  symbol{};
    Timestamp ts{};
};

constexpr EventHeader header_of(const MarketEvent& event) {
    return std::visit(
        [](const auto& e) -> EventHeader { return {e.kind, e.venue, e.symbol, e.ts}; }, event);
}

using OrderId  = std::uint64_t;  ///< Caller-assigned; unique per submitted Order.
using Notional = double;         ///< Quote-currency amount (fees, PnL).

/// A strategy's desired end-state for one symbol — a target position, not
/// a delta or a venue order ("be +2 BTC", not "buy 2 BTC"). RiskGate turns
/// this into concrete Order(s), computing the delta itself against the
/// current position. No ts field — same as Order, timestamps are
/// call-site parameters where actually consumed, not struct fields.
struct Intent {
    SymbolId symbol{};
    VenueId  venue{};  ///< Which leg (D44) — (symbol, venue) together identify an instrument;
                       ///< symbol alone doesn't, same reasoning as MarketEvent::venue (D43).
    Qty target_position{};  ///< Signed: positive = net long, negative = net short.
};

static_assert(sizeof(Intent) == 16, "unexpected padding/size regression");

/// A request to trade. Market order only — no price/type field yet; earns
/// its place when limit orders do.
struct Order {
    OrderId  id{};
    SymbolId symbol{};
    Side     side{};
    VenueId  venue{};  ///< Which leg (D44); see Intent::venue.
    Qty      qty{};
};

static_assert(sizeof(Order) == 24, "unexpected padding/size regression");

/// What happened to a submitted Order: filled. Field order is
/// alignment-driven (D32): symbol/side/venue grouped right after order_id
/// saves 8 bytes of padding (56 -> 48) versus declaration order; venue
/// (D44) slots into the same gap side already left, no size change.
struct Fill {
    OrderId   order_id{};
    SymbolId  symbol{};
    Side      side{};
    VenueId   venue{};  ///< Which leg (D44); see Intent::venue.
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
/// alignment-driven (D32): symbol/reason/venue grouped right after
/// order_id saves 8 bytes of padding (32 -> 24) versus declaration order;
/// venue (D44) slots into the same gap reason already left.
struct Reject {
    OrderId      order_id{};
    SymbolId     symbol{};
    RejectReason reason{};
    VenueId      venue{};  ///< Which leg (D44); see Intent::venue.
    Timestamp    ts{};
};

static_assert(sizeof(Reject) == 24, "unexpected padding/size regression");

}  // namespace qp
