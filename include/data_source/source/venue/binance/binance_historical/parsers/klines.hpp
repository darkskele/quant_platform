#pragma once
#include <string_view>

#include "csv_field.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one klines CSV row into ts and out. Reads the first seven columns and
/// ignores the rest. False leaves both untouched from the caller's view.
inline bool parse_klines_row(std::string_view row, Timestamp& ts, KlineEvent& out) noexcept {
    Timestamp  open_time{};
    KlineEvent kline{};

    if (!take_stamp(row, open_time)) return false;
    if (!take_decimal(row, kline.open)) return false;
    if (!take_decimal(row, kline.high)) return false;
    if (!take_decimal(row, kline.low)) return false;
    if (!take_decimal(row, kline.close)) return false;
    if (!take_decimal(row, kline.volume)) return false;
    if (!take_stamp(row, kline.close_time)) return false;

    ts  = open_time;
    out = kline;
    return true;
}

}  // namespace qp::data_source::source::venue::binance::parsers
