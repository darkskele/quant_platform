#pragma once
#include <string_view>

#include "csv_field.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// The first seven columns of the twelve column kline layout, shared by klines,
/// markPriceKlines and premiumIndexKlines.
struct KlineRow {
    Timestamp open_time{};
    Timestamp close_time{};
    double    open{};
    double    high{};
    double    low{};
    double    close{};
    double    volume{};
};

/// Reads the columns the events need and ignores the trailing five. False
/// leaves out untouched.
inline bool read_kline_row(std::string_view row, KlineRow& out) noexcept {
    KlineRow fields{};

    if (!take_stamp(row, fields.open_time)) return false;
    if (!take_decimal(row, fields.open)) return false;
    if (!take_decimal(row, fields.high)) return false;
    if (!take_decimal(row, fields.low)) return false;
    if (!take_decimal(row, fields.close)) return false;
    if (!take_decimal(row, fields.volume)) return false;
    if (!take_stamp(row, fields.close_time)) return false;

    out = fields;
    return true;
}

}  // namespace qp::data_source::source::venue::binance::parsers
