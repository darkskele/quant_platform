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

namespace qp::collector {

using SelectedParser = venue::binance::BinanceParser;

inline constexpr auto kDefaultWsEndpoint   = venue::binance::kFuturesWsProduction;
inline constexpr auto kDefaultWsTestnet    = venue::binance::kFuturesWsTestnet;
inline constexpr auto kDefaultRestEndpoint = venue::binance::kFuturesRestProduction;
inline constexpr auto kDefaultRestTestnet  = venue::binance::kFuturesRestTestnet;

}  // namespace qp::collector

#else
#error "apps/collector: unknown QP_COLLECTOR_VENUE (supported: binance)"
#endif
