#pragma once
#include <string_view>

#include "endpoints.hpp"
#include "kline_row.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one premiumIndexKlines CSV row into ts and out. The values are rates
/// and go negative. False leaves both untouched from the caller's view.
inline bool parse_premium_klines_row(std::string_view row, Timestamp& ts,
                                     PremiumIndexKlineEvent& out) noexcept {
    KlineRow fields{};
    if (!read_kline_row(row, fields)) return false;

    ts  = fields.open_time;
    out = PremiumIndexKlineEvent{.close_time = fields.close_time,
                                 .open       = fields.open,
                                 .high       = fields.high,
                                 .low        = fields.low,
                                 .close      = fields.close};
    return true;
}

struct PremiumIndexKlineParser {
    using Event = PremiumIndexKlineEvent;

    static constexpr EventKind    event_kind    = EventKind::PremiumIndexKline;
    static constexpr EndpointKind endpoint_kind = EndpointKind::PremiumIndexKlines;
    static constexpr bool         intervalled   = true;

    static bool parse(std::string_view row, Timestamp& ts, Event& out) noexcept {
        return parse_premium_klines_row(row, ts, out);
    }
};

}  // namespace qp::data_source::source::venue::binance::parsers
