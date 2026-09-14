#pragma once
#include <cstdint>

namespace qp {

/// Identifies a specific market within an exchange.
using MarketId = std::uint8_t;

namespace markets {

inline constexpr MarketId BinanceUsdm  = 0;
inline constexpr MarketId BinanceCoinm = 1;
inline constexpr MarketId BinanceSpot  = 2;

}  // namespace markets

}  // namespace qp
