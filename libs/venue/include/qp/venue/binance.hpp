#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "qp/core/types.hpp"

// Binance USD-M specifics, quarantined here so they never leak into the clean
// marketdata/execution interfaces. Adding a second venue is additive.
//
// Stateless translation only — no session/continuity state lives here. Gap
// detection (comparing MarketEvent::prev_seq against the last seq seen for a
// symbol) is the caller's job (LiveWebSocketSource owns that state).
namespace qp::venue::binance {

// Maps exchange symbol strings <-> internal SymbolId.
class SymbolTable {
public:
    SymbolId            intern(const std::string& sym);
    const std::string&  name(SymbolId id) const;

private:
    std::vector<std::string> names_;
};

struct WsEndpoint {
    std::string_view host;
    std::string_view port;
};
inline constexpr WsEndpoint kFuturesWsProduction{"fstream.binance.com", "443"};
inline constexpr WsEndpoint kFuturesWsTestnet{"stream.binancefuture.com", "443"};

struct RestEndpoint {
    std::string_view base_url;
};
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
                              WsEndpoint                       endpoint    = kFuturesWsProduction,
                              std::string_view                 depth_speed = "100ms");

// REST endpoint for the initial depth snapshot used to seed book
// reconstruction. Not yet called anywhere — snapshot+resync is follow-up work.
std::string depth_snapshot_url(std::string_view symbol, int limit,
                                RestEndpoint endpoint = kFuturesRestProduction);

// Parses one combined-stream message (`{"stream":...,"data":{...}}`) into
// `out`. Handles depthUpdate -> BookDiff and aggTrade -> Trade. Returns false
// for anything else (subscription acks, ping frames, unrecognized event
// types, malformed JSON) — caller should just skip the message.
bool parse_message(std::string_view msg, SymbolTable& symbols, MarketEvent& out);

}  // namespace qp::venue::binance
