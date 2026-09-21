#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>

namespace qp {

using Timestamp = std::int64_t;
using Price     = double;
using Qty       = double;
using Notional  = double;

enum class Side : std::uint8_t { Buy, Sell };

enum class EventKind : std::uint8_t {
    Trade,
    Funding,
    Kline,
    MarkPriceKline,
    PremiumIndexKline,
    OpenInterest,
    BookDepth,
    END
};

/// Kinds in EventKind. Masks over kinds size themselves off this, so it moves
/// with the last enumerator.
inline constexpr std::size_t kEventKindCount = static_cast<std::size_t>(EventKind::END);

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

/// Size resting between mid and one band's distance from it. Cumulative, so
/// band 5 includes everything inside band 1.
struct DepthBand {
    Qty      depth{};     ///< Contracts on Coin-M, base asset on USD-M.
    Notional notional{};  ///< Base coin on Coin-M, quote on USD-M, since Coin-M is inverse.
};

/// Binance's bookDepth sample, roughly every 30 seconds. Index k is the band
/// k + 1 percent from mid. @todo too binance specific, find some way to generalise
struct BookDepthBands {
    std::array<DepthBand, 5> bids;
    std::array<DepthBand, 5> asks;
};

/// Behind a shared_ptr, so ten bands inline do not widen every MarketEvent to
/// carry them.
/// @todo maybe switch for vector when we generalize
struct BookDepthEvent {
    std::shared_ptr<const BookDepthBands> bands;
};

using MarketEventPayload = std::variant<TradeEvent, FundingEvent, KlineEvent, MarkPriceKlineEvent,
                                        PremiumIndexKlineEvent, OpenInterestEvent, BookDepthEvent>;

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
