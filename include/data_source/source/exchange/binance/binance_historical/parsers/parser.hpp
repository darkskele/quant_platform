#pragma once
#include <concepts>
#include <string_view>

#include "endpoints.hpp"
#include "types.hpp"

namespace qp::data_source::source::exchange::binance::parsers {

/// The seam a stream reads rows through. Names the payload it produces, the
/// dataset it reads, and the event kind it stamps.
template <class P>
concept Parser =
    requires(const Endpoint& entry, std::string_view row, Timestamp& ts, typename P::Event& out) {
        { P::parse(entry, row, ts, out) } noexcept -> std::same_as<bool>;
        { P::event_kind } -> std::convertible_to<EventKind>;
        { P::endpoint_kind } -> std::convertible_to<EndpointKind>;
        { P::intervalled } -> std::convertible_to<bool>;
    };

}  // namespace qp::data_source::source::exchange::binance::parsers
