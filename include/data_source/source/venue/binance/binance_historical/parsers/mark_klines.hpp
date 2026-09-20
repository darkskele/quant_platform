#pragma once
#include <string_view>

#include "endpoints.hpp"
#include "kline_row.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one markPriceKlines CSV row into ts and out. The volume columns are
/// always zero on this dataset and are dropped. False leaves both untouched
/// from the caller's view.
inline bool parse_mark_klines_row(const Endpoint& entry, std::string_view row, Timestamp& ts,
                                  MarkPriceKlineEvent& out) noexcept {
    KlineRow fields{};
    if (!read_kline_row(entry, row, fields)) return false;

    ts  = fields.open_time;
    out = MarkPriceKlineEvent{.close_time = fields.close_time,
                              .open       = fields.open,
                              .high       = fields.high,
                              .low        = fields.low,
                              .close      = fields.close};
    return true;
}

struct MarkPriceKlineParser {
    using Event = MarkPriceKlineEvent;

    static constexpr EventKind    event_kind    = EventKind::MarkPriceKline;
    static constexpr EndpointKind endpoint_kind = EndpointKind::MarkPriceKlines;
    static constexpr bool         intervalled   = true;

    static bool parse(const Endpoint& entry, std::string_view row, Timestamp& ts,
                      Event& out) noexcept {
        return parse_mark_klines_row(entry, row, ts, out);
    }
};

}  // namespace qp::data_source::source::venue::binance::parsers
