#pragma once
#include <string_view>

#include "kline_row.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one markPriceKlines CSV row into ts and out. The volume columns are
/// always zero on this dataset and are dropped. False leaves both untouched
/// from the caller's view.
inline bool parse_mark_klines_row(std::string_view row, Timestamp& ts,
                                  MarkPriceKlineEvent& out) noexcept {
    KlineRow fields{};
    if (!read_kline_row(row, fields)) return false;

    ts  = fields.open_time;
    out = MarkPriceKlineEvent{.close_time = fields.close_time,
                              .open       = fields.open,
                              .high       = fields.high,
                              .low        = fields.low,
                              .close      = fields.close};
    return true;
}

}  // namespace qp::data_source::source::venue::binance::parsers
