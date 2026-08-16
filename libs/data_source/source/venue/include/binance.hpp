#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"
#include "venue_types.hpp"

// Binance USD-M specifics, quarantined here so they never leak into the clean
// marketdata/execution interfaces. Adding a second venue is additive.
//
// Stateless translation only — no session/continuity state lives here. Gap
// detection (comparing MarketEvent::prev_seq against the last seq seen for a
// symbol) is the caller's job (LiveWebSocketSource owns that state).
namespace qp::venue::binance {

// SymbolTable/WsEndpoint/RestEndpoint/DepthSnapshot have zero Binance-specific
// content (venue_types.hpp) — aliased here so this file and its
// tests keep reading in venue-local terms without duplicating the types.
using SymbolTable   = qp::source::SymbolTable;
using WsEndpoint    = qp::source::WsEndpoint;
using RestEndpoint  = qp::source::RestEndpoint;
using DepthSnapshot = qp::source::DepthSnapshot;

inline constexpr WsEndpoint kFuturesWsProduction{"fstream.binance.com", "443"};
inline constexpr WsEndpoint kFuturesWsTestnet{"stream.binancefuture.com", "443"};

inline constexpr RestEndpoint kFuturesRestProduction{"https://fapi.binance.com"};
inline constexpr RestEndpoint kFuturesRestTestnet{"https://testnet.binancefuture.com"};

// Path+query portion of the combined-stream URL, e.g. "/stream?streams=...".
// Split from the full URL because Beast wants host/port and target separately.
// `depth_speed` is Binance's diff-depth update-interval suffix (e.g. "100ms",
// "250ms") — kept as a plain string, not an enum, since it's exchange-detail
// churn rather than a seam; check Binance's current WS docs before changing
// the default.
std::string build_stream_path(const std::vector<std::string>& symbols,
                              std::string_view                depth_speed = "100ms");

// Full combined WebSocket stream URL for depth-diff + aggTrade, one connection
// for all symbols. `symbols` are exchange symbols, any case (lowercased here).
std::string build_stream_url(const std::vector<std::string>& symbols,
                             WsEndpoint                      endpoint    = kFuturesWsProduction,
                             std::string_view                depth_speed = "100ms");

// REST endpoint for the initial depth snapshot used to seed book
// reconstruction / resync after a gap.
std::string depth_snapshot_url(std::string_view symbol, int limit,
                               RestEndpoint endpoint = kFuturesRestProduction);

// Parses the REST response body from depth_snapshot_url. Pure parsing, no
// I/O — the actual HTTP fetch (Beast-dependent) lives in libs/data_source/source,
// same split as build_stream_path (URL/parsing here, transport there).
std::optional<DepthSnapshot> parse_depth_snapshot(std::string_view json_body);

// Parses one combined-stream message (`{"stream":...,"data":{...}}`) into
// `out`. Handles depthUpdate -> BookDiff and aggTrade -> Trade. Returns false
// for anything else (subscription acks, ping frames, unrecognized event
// types, malformed JSON) — caller should just skip the message.
bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out);

// Satisfies qp::source::Parser (libs/data_source/source/include/parser.hpp) —
// a thin static-method wrapper around the free functions above, not a
// reimplementation. Exists so GenericLiveWebSocketSource can be templated on
// a venue's wire-protocol glue without touching venue::binance's own
// (already tested) public API or its tests.
struct BinanceParser {
    static std::string build_stream_path(const std::vector<std::string>& symbols) {
        return ::qp::venue::binance::build_stream_path(symbols);
    }

    static bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out) {
        return ::qp::venue::binance::parse_message(msg, symbols, out);
    }

    static std::string depth_snapshot_url(std::string_view symbol, int limit,
                                          RestEndpoint endpoint) {
        return ::qp::venue::binance::depth_snapshot_url(symbol, limit, endpoint);
    }

    static std::optional<DepthSnapshot> parse_depth_snapshot(std::string_view json_body) {
        return ::qp::venue::binance::parse_depth_snapshot(json_body);
    }
};

}  // namespace qp::venue::binance
