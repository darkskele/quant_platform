#pragma once
#include <string_view>

#include "endpoints.hpp"
#include "kline_row.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one klines CSV row into ts and out. False leaves both untouched from
/// the caller's view.
inline bool parse_klines_row(const Endpoint& entry, std::string_view row, Timestamp& ts,
                             KlineEvent& out) noexcept {
    KlineRow fields{};
    if (!read_kline_row(entry, row, fields)) return false;

    ts  = fields.open_time;
    out = KlineEvent{.close_time = fields.close_time,
                     .open       = fields.open,
                     .high       = fields.high,
                     .low        = fields.low,
                     .close      = fields.close,
                     .volume     = fields.volume};
    return true;
}

struct KlineParser {
    using Event = KlineEvent;

    static constexpr EventKind    event_kind    = EventKind::Kline;
    static constexpr EndpointKind endpoint_kind = EndpointKind::Klines;
    static constexpr bool         intervalled   = true;

    static bool parse(const Endpoint& entry, std::string_view row, Timestamp& ts,
                      Event& out) noexcept {
        return parse_klines_row(entry, row, ts, out);
    }
};

}  // namespace qp::data_source::source::venue::binance::parsers
