#pragma once
#include <string_view>

#include "csv_field.hpp"
#include "endpoints.hpp"
#include "types.hpp"

namespace qp::data_source::source::venue::binance::parsers {

/// Parses one fundingRate CSV row into ts and out. False leaves both untouched
/// from the caller's view.
inline bool parse_funding_row(std::string_view row, Timestamp& ts, FundingEvent& out) noexcept {
    Timestamp    calc_time{};
    std::int64_t interval_hours{};
    double       rate{};

    if (!take_stamp(row, calc_time)) return false;
    if (!take_integer(row, interval_hours)) return false;
    if (!take_decimal(row, rate)) return false;
    if (interval_hours <= 0) return false;

    ts  = calc_time;
    out = FundingEvent{.funding_rate   = rate,
                       .interval_hours = static_cast<std::int32_t>(interval_hours)};
    return true;
}

struct FundingParser {
    using Event = FundingEvent;

    static constexpr EventKind    event_kind    = EventKind::Funding;
    static constexpr EndpointKind endpoint_kind = EndpointKind::FundingRate;
    static constexpr bool         intervalled   = false;

    static bool parse(std::string_view row, Timestamp& ts, Event& out) noexcept {
        return parse_funding_row(row, ts, out);
    }
};

}  // namespace qp::data_source::source::venue::binance::parsers
