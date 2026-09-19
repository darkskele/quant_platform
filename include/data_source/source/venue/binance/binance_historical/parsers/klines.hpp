#pragma once
#include <string_view>

#include "kline_row.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one klines CSV row into ts and out. False leaves both untouched from
/// the caller's view.
inline bool parse_klines_row(std::string_view row, Timestamp& ts, KlineEvent& out) noexcept {
    KlineRow fields{};
    if (!read_kline_row(row, fields)) return false;

    ts  = fields.open_time;
    out = KlineEvent{.close_time = fields.close_time,
                     .open       = fields.open,
                     .high       = fields.high,
                     .low        = fields.low,
                     .close      = fields.close,
                     .volume     = fields.volume};
    return true;
}

}  // namespace qp::data_source::source::venue::binance::parsers
