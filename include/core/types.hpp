#pragma once
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

#include "markets.hpp"

namespace qp {

using Timestamp = std::int64_t;   ///< Nanoseconds since epoch.
using Price     = double;         ///< @todo: fixed-point ticks for exactness.
using Qty       = double;         ///< Same open exactness gap as Price: step size, not tick size.
using SymbolId  = std::uint32_t;  ///< Index into market symbol table.

enum class Side : std::uint8_t { Buy, Sell };

struct PriceLevel {
    Price price{};
    Qty   qty{};  ///< 0 qty in a diff means the level was removed.
};

static_assert(
    std::is_trivially_copyable_v<PriceLevel>);  // safe to memcpy, wire format relies on this
static_assert(sizeof(PriceLevel) == 16, "unexpected padding/size regression");

/// A full-book-replace anchor. Means "clear the book, then
/// apply these" rather than "apply on top of what's there."
///
/// Kline: an OHLCV bar with a genuine live analog, so it's its own kind
/// rather than squeezed into Trade at close price.
enum class EventKind : std::uint8_t {
    BookDiff,
    Trade,
    Funding,
    BookSnapshot,
    Kline,
    MarkPriceKline
};

/// One flat struct per event kind, not one struct with every kind's
/// fields. First four members are always kind/market/symbol/ts, in that
/// order, so header_of() below reads the same way regardless of kind.
struct TradeEvent {
    EventKind kind = EventKind::Trade;
    MarketId  market{};
    SymbolId  symbol{};
    Timestamp ts{};
    Side      side{};
    Price     price{};
    Qty       qty{};
};

static_assert(std::is_trivially_copyable_v<TradeEvent>);

struct FundingEvent {
    EventKind kind = EventKind::Funding;
    MarketId  market{};
    SymbolId  symbol{};
    Timestamp ts{};
    double    funding_rate{};
};

static_assert(std::is_trivially_copyable_v<FundingEvent>);

struct KlineEvent {
    EventKind kind = EventKind::Kline;
    MarketId  market{};
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

/// Binance's own fair-value price for the contract, published independently
/// so funding settlement can't be gamed by spoofing the last trade price.
struct MarkPriceKlineEvent {
    EventKind kind = EventKind::MarkPriceKline;
    MarketId  market{};
    SymbolId  symbol{};
    Timestamp ts{};  ///< Bar open time.
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
};

static_assert(std::is_trivially_copyable_v<MarkPriceKlineEvent>);

/// BookDiff/BookSnapshot's actual levels. Behind a shared_ptr in both
/// event structs so N ring consumers can share one already-parsed
/// BookLevels instead of deep-copying it on every pop.
struct BookLevels {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

struct BookDiffEvent {
    EventKind                         kind = EventKind::BookDiff;
    MarketId                          market{};
    SymbolId                          symbol{};
    Timestamp                         ts{};
    std::uint64_t                     first_seq{};  ///< First seq in event (Binance's U).
    std::uint64_t                     seq{};        ///< Final update id (Binance's u).
    std::uint64_t                     prev_seq{};   ///< Continues-from seq (pu); 0 if n/a.
    std::shared_ptr<const BookLevels> levels;
};

struct BookSnapshotEvent {
    EventKind                         kind = EventKind::BookSnapshot;
    MarketId                          market{};
    SymbolId                          symbol{};
    Timestamp                         ts{};
    std::shared_ptr<const BookLevels> levels;
};

/// The lingua franca: every event this system moves around, one of six
/// kinds.
using MarketEvent = std::variant<TradeEvent, FundingEvent, KlineEvent, MarkPriceKlineEvent,
                                 BookDiffEvent, BookSnapshotEvent>;

/// Just the fields every kind shares, read via std::visit since
/// std::variant gives no safe way to peek a common prefix. What a
/// multi-ring merge (ordering by ts) needs without caring which kind it's
/// looking at.
struct EventHeader {
    EventKind kind{};
    MarketId  market{};
    SymbolId  symbol{};
    Timestamp ts{};
};

constexpr EventHeader header_of(const MarketEvent& event) {
    return std::visit(
        [](const auto& e) -> EventHeader { return {e.kind, e.market, e.symbol, e.ts}; }, event);
}

using OrderId  = std::uint64_t;  ///< Caller-assigned; unique per submitted Order.
using Notional = double;         ///< Quote-currency amount (fees, PnL).

/// A strategy's desired end-state for one symbol: a target position, not
/// a delta or a market order ("be +2 BTC", not "buy 2 BTC"). RiskGate turns
/// this into concrete Order(s), computing the delta itself.
struct Intent {
    SymbolId symbol{};
    MarketId market{};           ///< Which market. (symbol, market) together identify an instrument.
    Qty      target_position{};  ///< Signed: positive = net long, negative = net short.
};

static_assert(sizeof(Intent) == 16, "unexpected padding/size regression");

/// A request to trade. Market order only, no price/type field yet.
struct Order {
    OrderId  id{};
    SymbolId symbol{};
    Side     side{};
    MarketId market{};  ///< Which market; see Intent::market.
    Qty      qty{};
};

static_assert(sizeof(Order) == 24, "unexpected padding/size regression");

/// What happened to a submitted Order: filled. symbol/side/market sit right
/// after order_id, saving 8 bytes of padding versus declaration order.
struct Fill {
    OrderId   order_id{};
    SymbolId  symbol{};
    Side      side{};
    MarketId  market{};  ///< Which market; see Intent::market.
    Timestamp ts{};
    Price     price{};
    Qty       qty{};  ///< == Order::qty always for now, no partials.
    Notional  fee{};
};

static_assert(sizeof(Fill) == 48, "unexpected padding/size regression");

/// Reasons grow as real ones appear.
enum class RejectReason : std::uint8_t { NoPriceAvailable, NoCostAvailable };

/// What happened to a submitted Order: didn't. symbol/reason/market sit
/// right after order_id, saving 8 bytes of padding versus declaration order.
struct Reject {
    OrderId      order_id{};
    SymbolId     symbol{};
    RejectReason reason{};
    MarketId     market{};  ///< Which market; see Intent::market.
    Timestamp    ts{};
};

static_assert(sizeof(Reject) == 24, "unexpected padding/size regression");

}  // namespace qp
