#pragma once
#include <array>
#include <cstddef>
#include <tuple>

#include "bin_hist_symbol_table.hpp"
#include "portfolio.hpp"

namespace qp::backtest::config {

// The venue/symbol universe for this backtest binary, declared exactly
// once: index i == VenueId i, matching run_data_source's own positional
// source/sink pairing (D48). Everything downstream that needs to know "how
// many symbols does venue i have" (Book, LegSource<N>'s Parser, whatever
// pool BasicRiskGate/LastTradeMatcher size off Book::kMaxInstruments)
// derives it from this tuple instead of repeating the number.
using FuturesTable = data_source::source::venue::binance::binance_historical::BinHistSymbolTable;
using SpotTable    = data_source::source::venue::binance::binance_historical::BinHistSymbolTable;
using VenueTables  = std::tuple<FuturesTable, SpotTable>;

template <class... Tables>
consteval std::array<std::size_t, sizeof...(Tables)> counts_of(std::tuple<Tables...>) {
    return {Tables::kSymbols.size()...};
}

inline constexpr auto kCounts = counts_of(VenueTables{});
using Book                    = qp::Portfolio<kCounts>;

}  // namespace qp::backtest::config
