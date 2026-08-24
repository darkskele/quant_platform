#pragma once

// The one file allowed to name a concrete venue (apps/* composition-root
// rule — see docs/architecture-principles.md). QP_COLLECTOR_VENUE (CMake
// option, apps/collector/CMakeLists.txt) selects which branch compiles via
// a QP_VENUE_* define; collector.hpp/.cpp read only the generic names below
// and never mention venue::binance (or a future venue::kraken) directly.
// Adding a venue is additive: a new #elif branch here, nothing in
// collector.hpp/.cpp changes.

#if defined(QP_VENUE_BINANCE)

#include "binance.hpp"
#include "resync_policy.hpp"

namespace qp::collector {

using SelectedParser    = venue::binance::BinanceParser<venue::binance::FuturesMarket>;
using SelectedAlignment = source::FuturesAlignment;  // D13/D36: USD-M futures only today

inline constexpr auto kDefaultWsEndpoint   = venue::binance::kFuturesWsProduction;
inline constexpr auto kDefaultWsTestnet    = venue::binance::kFuturesWsTestnet;
inline constexpr auto kDefaultRestEndpoint = venue::binance::kFuturesRestProduction;
inline constexpr auto kDefaultRestTestnet  = venue::binance::kFuturesRestTestnet;

// D40/D41: the collector's second leg — carry needs both spot and perp.
// Kept alongside the futures selections above rather than replacing them
// (existing names stay futures-scoped, smallest diff).
using SelectedSpotParser    = venue::binance::BinanceParser<venue::binance::SpotMarket>;
using SelectedSpotAlignment = source::SpotAlignment;

inline constexpr auto kDefaultSpotWsEndpoint   = venue::binance::kSpotWsProduction;
inline constexpr auto kDefaultSpotWsTestnet    = venue::binance::kSpotWsTestnet;
inline constexpr auto kDefaultSpotRestEndpoint = venue::binance::kSpotRestProduction;
inline constexpr auto kDefaultSpotRestTestnet  = venue::binance::kSpotRestTestnet;

}  // namespace qp::collector

#else
#error "apps/collector: unknown QP_COLLECTOR_VENUE (supported: binance)"
#endif
