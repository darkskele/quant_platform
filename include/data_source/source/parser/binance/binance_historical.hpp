#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"
#include "venue_types.hpp"

// Binance USD-M + spot specifics, quarantined here so they never leak into
// the clean marketdata/execution interfaces. Adding a second venue
// (non-Binance) is additive.
//
// Stateless translation only — no session/continuity state lives here. Gap
// detection (comparing MarketEvent::prev_seq/first_seq against the last seq
// seen for a symbol, per AlignmentRule) is the caller's job
// (LiveWebSocketSource owns that state).
namespace qp::data_source::source::parser::binance {

// SymbolTable/WsEndpoint/RestEndpoint/DepthSnapshot have zero Binance-specific
// content (venue_types.hpp) — aliased here so this file and its
// tests keep reading in venue-local terms without duplicating the types.
using SymbolTable   = qp::data_source::source::SymbolTable;
using WsEndpoint    = qp::data_source::source::WsEndpoint;
using RestEndpoint  = qp::data_source::source::RestEndpoint;
using DepthSnapshot = qp::data_source::source::DepthSnapshot;

inline constexpr WsEndpoint kFuturesWsProduction{"fstream.binance.com", "443"};
inline constexpr WsEndpoint kFuturesWsTestnet{"stream.binancefuture.com", "443"};

inline constexpr RestEndpoint kFuturesRestProduction{"https://fapi.binance.com"};
inline constexpr RestEndpoint kFuturesRestTestnet{"https://testnet.binancefuture.com"};

// D40: verified against Binance's own spot docs, not assumed from futures
// (D13's lesson). Port 9443 is spot's documented alternate WS port; 443
// works too (Binance docs list both) — kept explicit since futures already
// states its port explicitly the same way.
inline constexpr WsEndpoint kSpotWsProduction{"stream.binance.com", "9443"};
inline constexpr WsEndpoint kSpotWsTestnet{"stream.testnet.binance.vision", "443"};

inline constexpr RestEndpoint kSpotRestProduction{"https://api.binance.com"};
inline constexpr RestEndpoint kSpotRestTestnet{"https://testnet.binance.vision"};

// The one place spot and futures differ in wire-protocol *construction*
// (D40) — verified against Binance's docs that everything else
// (parse_message, parse_depth_snapshot, the combined-stream URL format,
// the @100ms depth-speed suffix, aggTrade's field set) is identical and
// shared, not assumed. kSubscribesFunding gates whether build_stream_path
// appends a @markPrice stream — spot has no funding concept, nothing to
// subscribe to. kRestPathPrefix is the REST snapshot path segment
// (/fapi/v1 vs /api/v3).
template <class T>
concept BinanceMarket = requires {
    { T::kRestPathPrefix } -> std::convertible_to<std::string_view>;
    { T::kSubscribesFunding } -> std::convertible_to<bool>;
};

struct FuturesMarket {
    static constexpr std::string_view kRestPathPrefix    = "/fapi/v1";
    static constexpr bool             kSubscribesFunding = true;
};

struct SpotMarket {
    static constexpr std::string_view kRestPathPrefix    = "/api/v3";
    static constexpr bool             kSubscribesFunding = false;
};

// Path+query portion of the combined-stream URL, e.g. "/stream?streams=...".
// Split from the full URL because Beast wants host/port and target separately.
// `depth_speed` is Binance's diff-depth update-interval suffix (e.g. "100ms",
// "250ms") — kept as a plain string, not an enum, since it's exchange-detail
// churn rather than a seam; check Binance's current WS docs before changing
// the default.
template <BinanceMarket M>
std::string build_stream_path(const std::vector<std::string>& symbols,
                              std::string_view                depth_speed = "100ms");

// Full combined WebSocket stream URL for depth-diff + aggTrade (+ markPrice
// on markets that have funding), one connection for all symbols. `symbols`
// are exchange symbols, any case (lowercased here). No default endpoint —
// deliberate (D38's precedent): a market-parameterized function silently
// falling back to a futures-shaped default is exactly the class of mistake
// this policy exists to catch at the call site instead.
template <BinanceMarket M>
std::string build_stream_url(const std::vector<std::string>& symbols, WsEndpoint endpoint,
                             std::string_view depth_speed = "100ms");

// REST endpoint for the initial depth snapshot used to seed book
// reconstruction / resync after a gap. No default endpoint, same reasoning
// as build_stream_url.
template <BinanceMarket M>
std::string depth_snapshot_url(std::string_view symbol, int limit, RestEndpoint endpoint);

extern template std::string build_stream_path<FuturesMarket>(const std::vector<std::string>&,
                                                             std::string_view);
extern template std::string build_stream_path<SpotMarket>(const std::vector<std::string>&,
                                                          std::string_view);
extern template std::string build_stream_url<FuturesMarket>(const std::vector<std::string>&,
                                                            WsEndpoint, std::string_view);
extern template std::string build_stream_url<SpotMarket>(const std::vector<std::string>&,
                                                         WsEndpoint, std::string_view);
extern template std::string depth_snapshot_url<FuturesMarket>(std::string_view, int, RestEndpoint);
extern template std::string depth_snapshot_url<SpotMarket>(std::string_view, int, RestEndpoint);

// Parses the REST response body from depth_snapshot_url. Pure parsing, no
// I/O — the actual HTTP fetch (Beast-dependent) lives in libs/data_source/source,
// same split as build_stream_path (URL/parsing here, transport there).
// Market-agnostic: the response shape ({lastUpdateId, bids, asks}) is
// identical on spot and futures — verified, not assumed.
std::optional<DepthSnapshot> parse_depth_snapshot(std::string_view json_body);

// Parses one combined-stream message (`{"stream":...,"data":{...}}`) into
// `out`. Handles depthUpdate -> BookDiff, aggTrade -> Trade, and
// markPriceUpdate -> Funding (mark_price + funding_rate together — Binance
// sends both in the same message). Returns false for anything else
// (subscription acks, ping frames, unrecognized event types, malformed
// JSON) — caller should just skip the message. Market-agnostic (D40):
// spot's depthUpdate carries the same b/a/U/u fields with no `pu` (reads
// as 0, handled by AlignmentRule::continues, not here); spot's aggTrade
// fields match futures' exactly; spot never sends markPriceUpdate at all
// since build_stream_path<SpotMarket> never subscribes to it — verified
// against Binance's docs, no market parameter needed on this function.
bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out);

// Satisfies qp::source::Parser (libs/data_source/source/include/parser.hpp) —
// a thin static-method wrapper around the free functions above, not a
// reimplementation. Exists so GenericLiveWebSocketSource can be templated on
// a venue's wire-protocol glue without touching venue::binance's own
// (already tested) public API or its tests. Templated on BinanceMarket so
// BinanceParser<FuturesMarket> and BinanceParser<SpotMarket> are distinct
// Parser-satisfying types, each with a market baked into build_stream_path/
// depth_snapshot_url at compile time — no runtime market flag to wire wrong.
template <BinanceMarket M>
struct BinanceParser {
    static std::string build_stream_path(const std::vector<std::string>& symbols) {
        return ::qp::data_source::source::parser::binance::build_stream_path<M>(symbols);
    }

    static bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out) {
        return ::qp::data_source::source::parser::binance::parse_message(msg, symbols, out);
    }

    static std::string depth_snapshot_url(std::string_view symbol, int limit,
                                          RestEndpoint endpoint) {
        return ::qp::data_source::source::parser::binance::depth_snapshot_url<M>(symbol, limit, endpoint);
    }

    static std::optional<DepthSnapshot> parse_depth_snapshot(std::string_view json_body) {
        return ::qp::data_source::source::parser::binance::parse_depth_snapshot(json_body);
    }
};

}  // namespace qp::data_source::source::parser::binance
