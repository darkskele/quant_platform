#pragma once
#include <array>
#include <cstddef>

#include "bin_hist_symbol_table.hpp"
#include "markets.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"

namespace qp::backtest::config {

// The market universe for this backtest binary.
using FuturesTable = data_source::source::venue::binance::binance_historical::BinHistSymbolTable;
using SpotTable    = data_source::source::venue::binance::binance_historical::BinHistSymbolTable;

// Which Market each positional leg (source[i]/sink[i]) is for.
inline constexpr std::array<Market, 2> kLegMarkets{Market::BinanceUsdm, Market::BinanceSpot};

inline constexpr std::size_t kNumLegs = kLegMarkets.size();

inline SubscriptionConfig make_subscription() {
    SubscriptionConfig sub{};
    sub.set(Market::BinanceUsdm, FuturesTable::kSymbols.size());
    sub.set(Market::BinanceSpot, SpotTable::kSymbols.size());
    return sub;
}

using Book = qp::Portfolio;

}  // namespace qp::backtest::config
