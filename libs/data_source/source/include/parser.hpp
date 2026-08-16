#pragma once
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"
#include "venue_types.hpp"

namespace qp::source {

// The minimal shape GenericLiveWebSocketSource needs from a venue's wire-
// protocol glue — exactly the 4 operations it calls today, nothing added
// for a venue that doesn't exist. venue::binance::BinanceParser is the sole
// implementation today.
//
// SymbolTable, RestEndpoint, and DepthSnapshot (qp/venue_types.hpp)
// are venue-agnostic types, not venue::binance-specific ones renamed —
// content has zero Binance-specific logic (SymbolTable is pure string<->id
// interning; DepthSnapshot is just {last_update_id, bids, asks}), so any
// Parser can use them directly without naming a venue namespace.
template <class P>
concept Parser = requires(const std::vector<std::string>& symbols, std::string_view msg,
                          SymbolTable& table, MarketEvent& ev, std::string_view symbol_name,
                          int limit, RestEndpoint endpoint, std::string_view body) {
    { P::build_stream_path(symbols) } -> std::convertible_to<std::string>;
    { P::parse_message(msg, table, ev) } -> std::convertible_to<bool>;
    { P::depth_snapshot_url(symbol_name, limit, endpoint) } -> std::convertible_to<std::string>;
    { P::parse_depth_snapshot(body) } -> std::same_as<std::optional<DepthSnapshot>>;
};

}  // namespace qp::source
