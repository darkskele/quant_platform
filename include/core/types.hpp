#pragma once
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

namespace qp {

using Timestamp = std::int64_t;
using Price     = double;
using Qty       = double;
using Notional  = double;

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
    MarkPriceKline,
    PremiumIndexKline,
    OpenInterest
};

struct EventBase {
    EventKind     kind{};
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
    Timestamp     ts{};
};

static_assert(std::is_trivially_copyable_v<EventBase>);

struct TradeEvent {
    Side  side{};
    Price price{};
    Qty   qty{};
};

struct FundingEvent {
    double       funding_rate{};
    std::int32_t interval_hours{8};
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

struct PremiumIndexKlineEvent {
    Timestamp close_time{};
    Price     open{};
    Price     high{};
    Price     low{};
    Price     close{};
};

/// Binance's metrics dataset, sampled every five minutes. Coin-M publishes the
/// open interest columns but leaves the three long short ratios empty, so any
/// ratio here can be NaN and every reader has to check before using one.
struct OpenInterestEvent {
    Qty      open_interest{};        ///< Contracts on Coin-M, base asset on USD-M.
    Notional open_interest_value{};  ///< Same figure in quote terms.
    double   toptrader_account_ratio{};
    double   toptrader_position_ratio{};
    double   account_long_short_ratio{};
    double   taker_long_short_volume_ratio{};
};

static_assert(std::is_trivially_copyable_v<OpenInterestEvent>);

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

using MarketEventPayload =
    std::variant<TradeEvent, FundingEvent, KlineEvent, MarkPriceKlineEvent, PremiumIndexKlineEvent,
                 OpenInterestEvent, BookDiffEvent, BookSnapshotEvent>;

struct MarketEvent {
    EventBase          base{};
    MarketEventPayload payload{};
};

using OrderId = std::uint64_t;

struct Intent {
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
    Qty           target_position{};
};

static_assert(std::is_trivially_copyable_v<Intent>);

struct Order {
    OrderId       id{};
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
    Side          side{};
    Qty           qty{};
};

static_assert(std::is_trivially_copyable_v<Order>);

struct Fill {
    OrderId       order_id{};
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
    Side          side{};
    Timestamp     ts{};
    Price         price{};
    Qty           qty{};
    Notional      fee{};
};

static_assert(std::is_trivially_copyable_v<Fill>);

enum class RejectReason : std::uint8_t { NoPriceAvailable, NoCostAvailable };

struct Reject {
    OrderId       order_id{};
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
    RejectReason  reason{};
    Timestamp     ts{};
};

static_assert(std::is_trivially_copyable_v<Reject>);

}  // namespace qp
