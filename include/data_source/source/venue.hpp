#pragma once
#include "types.hpp"

namespace qp::data_source::source {

/// Every venue/market this codebase has actually integrated with — grows
/// only when a new integration is built (a Kraken venue, say), not per-run
/// like SymbolId (venue_types.hpp's SymbolTable is the open-ended,
/// runtime-assigned one; this is the small, closed, code-defined one).
/// Named values for MarketEvent::venue (D43): FanoutSink::record<I>()
/// stamps the raw VenueId, but callers picking which index to record to,
/// or reading event.venue back downstream (Strategy, eventually Portfolio),
/// want a name instead of a bare integer.
///
/// Binance-only today, same as everything else in this town — a second
/// venue is additive here too, not a rewrite.
enum class Venue : VenueId {
    BinanceFutures = 0,
    BinanceSpot    = 1,
};

}  // namespace qp::data_source::source
