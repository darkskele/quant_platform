#pragma once
#include <cstdint>
#include <string_view>

#include "csv_field.hpp"
#include "endpoints.hpp"
#include "types.hpp"

namespace qp::data_source::source::exchange::binance::parsers {

/// Parses one aggTrades CSV row into ts and out. False leaves both untouched
/// from the caller's view.
///
/// The stamp is the sixth field, so five are read before there is a time.
/// Spot carries an eighth, is_best_match, which the endpoint's column count
/// says to expect.
inline bool parse_agg_trades_row(const Endpoint& entry, std::string_view row, Timestamp& ts,
                                 TradeEvent& out) noexcept {
    std::int64_t agg_id{};
    double       price{};
    double       qty{};
    std::int64_t first_id{};
    std::int64_t last_id{};
    Timestamp    stamp{};
    bool         buyer_maker{};
    if (!take_integer(row, agg_id)) return false;
    if (!take_decimal(row, price)) return false;
    if (!take_decimal(row, qty)) return false;
    if (!take_integer(row, first_id)) return false;
    if (!take_integer(row, last_id)) return false;
    if (!take_stamp(row, stamp)) return false;
    if (!take_bool(row, buyer_maker)) return false;
    if (entry.column_count > 7) {
        bool best_match{};
        if (!take_bool(row, best_match)) return false;
    }
    if (!row.empty()) return false;

    if (price <= 0.0 || qty <= 0.0 || agg_id < 0 || first_id < 0 || last_id < first_id)
        return false;

    ts  = stamp;
    out = TradeEvent{.id             = static_cast<std::uint64_t>(agg_id),
                     .first_trade_id = static_cast<std::uint64_t>(first_id),
                     .last_trade_id  = static_cast<std::uint64_t>(last_id),
                     .price          = price,
                     .qty            = qty,
                     .side           = buyer_maker ? Side::Sell : Side::Buy};
    return true;
}

struct AggTradesParser {
    using Event = TradeEvent;

    static constexpr EventKind    event_kind    = EventKind::Trade;
    static constexpr EndpointKind endpoint_kind = EndpointKind::AggTrades;
    static constexpr bool         intervalled   = false;

    /// Aggregate ids run consecutively per symbol, so a break in them is missing
    /// tape. The stream counts those breaks off this.
    static std::uint64_t sequence(const Event& trade) noexcept { return trade.id; }

    static bool parse(const Endpoint& entry, std::string_view row, Timestamp& ts,
                      Event& out) noexcept {
        return parse_agg_trades_row(entry, row, ts, out);
    }
};

}  // namespace qp::data_source::source::exchange::binance::parsers
