#pragma once
#include <cstddef>
#include <cstdint>

namespace qp {

/// The closed universe of markets this platform trades.
enum class Market : std::uint8_t {
    BinanceUsdm  = 0,
    BinanceCoinm = 1,
    BinanceSpot  = 2,
};

/// Cardinality of the Market enum. Every per-market array is sized off this;
/// bump when Market gains a value.
inline constexpr std::size_t kNumMarkets = 3;

}  // namespace qp
