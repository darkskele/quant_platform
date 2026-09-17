#pragma once
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

namespace qp {

using Timestamp  = std::int64_t;
using Price      = double;
using Qty        = double;
using SlotOffset = std::uint16_t;

enum class Side : std::uint8_t { Buy, Sell };

struct PriceLevel {
    Price price{};
    Qty   qty{};
};

static_assert(std::is_trivially_copyable_v<PriceLevel>);
static_assert(sizeof(PriceLevel) == 16, "unexpected padding/size regression");

enum class EventKind : std::uint8_t {
    BookDiff,
    Trade,
    Funding,
    BookSnapshot,
    Kline,
    MarkPriceKline
};

struct EventBase {
    EventKind  kind{};
    SlotOffset exchange{};
    SlotOffset market{};
    SlotOffset symbol{};
    Timestamp  ts{};
};

static_assert(std::is_trivially_copyable_v<EventBase>);

struct TradeEvent {
    Side  side{};
    Price price{};
    Qty   qty{};
};

struct FundingEvent {
    double funding_rate{};
};

struct KlineEvent {
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
    Qty       volume{};
};

struct MarkPriceKlineEvent {
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
};

struct BookLevels {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
};

struct BookDiffEvent {
    std::uint64_t                     first_seq{};
    std::uint64_t                     seq{};
    std::uint64_t                     prev_seq{};
    std::shared_ptr<const BookLevels> levels;
};

struct BookSnapshotEvent {
    std::shared_ptr<const BookLevels> levels;
};

using MarketEventPayload = std::variant<TradeEvent, FundingEvent, KlineEvent, MarkPriceKlineEvent,
                                        BookDiffEvent, BookSnapshotEvent>;

struct MarketEvent {
    EventBase          base{};
    MarketEventPayload payload{};
};

using OrderId  = std::uint64_t;
using Notional = double;

struct Intent {
    SlotOffset exchange{};
    SlotOffset market{};
    SlotOffset symbol{};
    Qty        target_position{};
};

static_assert(std::is_trivially_copyable_v<Intent>);

struct Order {
    OrderId    id{};
    SlotOffset exchange{};
    SlotOffset market{};
    SlotOffset symbol{};
    Side       side{};
    Qty        qty{};
};

static_assert(std::is_trivially_copyable_v<Order>);

struct Fill {
    OrderId    order_id{};
    SlotOffset exchange{};
    SlotOffset market{};
    SlotOffset symbol{};
    Side       side{};
    Timestamp  ts{};
    Price      price{};
    Qty        qty{};
    Notional   fee{};
};

static_assert(std::is_trivially_copyable_v<Fill>);

enum class RejectReason : std::uint8_t { NoPriceAvailable, NoCostAvailable };

struct Reject {
    OrderId      order_id{};
    SlotOffset   exchange{};
    SlotOffset   market{};
    SlotOffset   symbol{};
    RejectReason reason{};
    Timestamp    ts{};
};

static_assert(std::is_trivially_copyable_v<Reject>);

}  // namespace qp
