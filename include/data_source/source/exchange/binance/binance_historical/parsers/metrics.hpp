#pragma once
#include <string_view>

#include "csv_field.hpp"
#include "endpoints.hpp"
#include "types.hpp"

namespace qp::data_source::source::exchange::binance::parsers {

/// Parses one metrics CSV row into ts and out.
inline bool parse_metrics_row(std::string_view row, Timestamp& ts,
                              OpenInterestEvent& out) noexcept {
    Timestamp create_time{};
    if (!take_datetime(row, create_time)) return false;
    if (take_field(row).empty()) return false;

    // Open interest is the reason this dataset is read, so a blank there is a
    // bad row. The four ratios are not, and Coin-M leaves three of them empty.
    double open_interest{};
    double open_interest_value{};
    if (!take_decimal(row, open_interest)) return false;
    if (!take_decimal(row, open_interest_value)) return false;

    double toptrader_account{};
    double toptrader_position{};
    double account_long_short{};
    double taker_long_short{};
    if (!take_optional_decimal(row, toptrader_account)) return false;
    if (!take_optional_decimal(row, toptrader_position)) return false;
    if (!take_optional_decimal(row, account_long_short)) return false;
    if (!take_optional_decimal(row, taker_long_short)) return false;

    ts  = create_time;
    out = OpenInterestEvent{.open_interest                 = open_interest,
                            .open_interest_value           = open_interest_value,
                            .toptrader_account_ratio       = toptrader_account,
                            .toptrader_position_ratio      = toptrader_position,
                            .account_long_short_ratio      = account_long_short,
                            .taker_long_short_volume_ratio = taker_long_short};
    return true;
}

struct MetricsParser {
    using Event = OpenInterestEvent;

    static constexpr EventKind    event_kind    = EventKind::OpenInterest;
    static constexpr EndpointKind endpoint_kind = EndpointKind::Metrics;
    static constexpr bool         intervalled   = false;

    static bool parse(const Endpoint&, std::string_view row, Timestamp& ts, Event& out) noexcept {
        return parse_metrics_row(row, ts, out);
    }
};

}  // namespace qp::data_source::source::exchange::binance::parsers
