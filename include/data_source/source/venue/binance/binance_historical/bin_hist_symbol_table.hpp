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

/// Binance Historical's fixed symbol universe.
using BinHistSymbolTable = FixedSymbolTable<detail::kSymbols>;

static_assert(qp::data_source::source::venue::SymbolTable<BinHistSymbolTable>);

}  // namespace qp::data_source::source::venue::binance::binance_historical
