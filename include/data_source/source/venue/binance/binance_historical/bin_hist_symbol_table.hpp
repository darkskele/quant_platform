#pragma once
#include <array>

#include "venue.hpp"

namespace qp::data_source::source::venue::binance::binance_historical {

namespace detail {
// 8 chars: the longest of the 10 (DOGEUSDT/LINKUSDT/AVAXUSDT).
inline constexpr std::array<FixedString<8>, 10> kSymbols = {
    "BTCUSDT",  "ETHUSDT", "SOLUSDT",  "BNBUSDT",  "XRPUSDT",
    "DOGEUSDT", "ADAUSDT", "LINKUSDT", "AVAXUSDT", "LTCUSDT",
};
}  // namespace detail

/// Binance Historical's fixed symbol universe: the top 10 USDT-margined
/// majors by the obvious liquidity ranking (funding-carry majors, matching
/// qp::Portfolio's own "solo retail portfolio" doc comment), each verified
/// (2026-08-29, not assumed) to have: a live USD-M PERPETUAL contract
/// (fapi.binance.com/fapi/v1/exchangeInfo, contractType=PERPETUAL,
/// status=TRADING), a live spot pair (api.binance.com/api/v3/exchangeInfo,
/// status=TRADING), and historical daily klines on data.binance.vision for
/// both legs. Extend detail::kSymbols, nothing else, when the actual
/// traded universe changes. The id_of/name_of algorithm itself lives in
/// venue::FixedSymbolTable (venue.hpp): nothing venue-specific about
/// it, only the data is Binance-specific.
using BinHistSymbolTable = FixedSymbolTable<detail::kSymbols>;

static_assert(qp::data_source::source::venue::SymbolTable<BinHistSymbolTable>);

}  // namespace qp::data_source::source::venue::binance::binance_historical
