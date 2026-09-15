#pragma once
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

#include "exchange.hpp"
#include "markets.hpp"

namespace qp {

using Timestamp = std::int64_t;   ///< Nanoseconds since epoch.
using Price     = double;         ///< @todo: fixed-point ticks for exactness.
using Qty       = double;         ///< Same open exactness gap as Price.
using SymbolId  = std::uint32_t;  ///< Index into market symbol table.

enum class Side : std::uint8_t { Buy, Sell };

struct PriceLevel {
    Price price{};
    Qty   qty{};  ///< 0 qty in a diff means the level was removed.
};

static_assert(
    std::is_trivially_copyable_v<PriceLevel>); 
static_assert(sizeof(PriceLevel) == 16, "unexpected padding/size regression");

enum class EventKind : std::uint8_t {
    BookDiff,
    Trade,
    Funding,
    BookSnapshot,
    Kline,
    MarkPriceKline
};

/// The route/identity fields every event carries.
struct EventBase {
    EventKind  kind{};
    ExchangeId exchange{};
    Market     market{};
    SymbolId   symbol{};
    Timestamp  ts{};
};

static_assert(std::is_trivially_copyable_v<EventBase>);

struct TradeEvent {
    EventBase base{.kind = EventKind::Trade};
    Side      side{};
    Price     price{};
    Qty       qty{};
};

static_assert(std::is_trivially_copyable_v<TradeEvent>);

struct FundingEvent {
    EventBase base{.kind = EventKind::Funding};
    double    funding_rate{};
};

static_assert(std::is_trivially_copyable_v<FundingEvent>);

struct KlineEvent {
    EventBase base{.kind = EventKind::Kline};
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
    Qty       volume{};
};

static_assert(std::is_trivially_copyable_v<KlineEvent>);

/// Binance's own fair-value price for the contract.  
/// @todo is this exchange specific?
struct MarkPriceKlineEvent {
    EventBase base{.kind = EventKind::MarkPriceKline};
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
};

static_assert(std::is_trivially_copyable_v<MarkPriceKlineEvent>);

/// BookDiff/BookSnapshot's actual levels.
struct BookLevels {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

struct BookDiffEvent {
    EventBase                         base{.kind = EventKind::BookDiff};
    std::uint64_t                     first_seq{};  ///< First seq in event.
    std::uint64_t                     seq{};        ///< Final update id.
    std::uint64_t                     prev_seq{};   ///< Continues-from seq (pu); 0 if n/a.
    std::shared_ptr<const BookLevels> levels;
};

struct BookSnapshotEvent {
    EventBase                         base{.kind = EventKind::BookSnapshot};
    std::shared_ptr<const BookLevels> levels;
};

using MarketEvent = std::variant<TradeEvent, FundingEvent, KlineEvent, MarkPriceKlineEvent,
                                 BookDiffEvent, BookSnapshotEvent>;

constexpr EventBase base_of(const MarketEvent& event) {
    return std::visit([](const auto& e) -> EventBase { return e.base; }, event);
}

using OrderId  = std::uint64_t;  ///< Caller-assigned.
using Notional = double;         ///< Quote-currency amount (fees, PnL).

/// A strategy's desired end-state for one symbol.
struct Intent {
    SymbolId symbol{};
    Market   market{};  ///< Which market. (symbol, market) together identify an instrument.
    Qty      target_position{};  ///< Signed: positive = net long, negative = net short.
};

static_assert(sizeof(Intent) == 16, "unexpected padding/size regression");

/// A request to trade. Market order only, no price/type field yet.
struct Order {
    OrderId  id{};
    SymbolId symbol{};
    Side     side{};
    Market   market{};  ///< Which market; see Intent::market.
    Qty      qty{};
};

static_assert(sizeof(Order) == 24, "unexpected padding/size regression");

/// What happened to a submitted Order.
struct Fill {
    OrderId   order_id{};
    SymbolId  symbol{};
    Side      side{};
    Market    market{};  ///< Which market; see Intent::market.
    Timestamp ts{};
    Price     price{};
    Qty       qty{};  ///< == Order::qty always for now, no partials.
    Notional  fee{};
};

static_assert(sizeof(Fill) == 48, "unexpected padding/size regression");

/// Reasons grow as real ones appear.
enum class RejectReason : std::uint8_t { NoPriceAvailable, NoCostAvailable };

/// What happened to a submitted Order.
struct Reject {
    OrderId      order_id{};
    SymbolId     symbol{};
    RejectReason reason{};
    Market       market{};  ///< Which market; see Intent::market.
    Timestamp    ts{};
};

static_assert(sizeof(Reject) == 24, "unexpected padding/size regression");

}  // namespace qp
