#pragma once
#include <cstdint>

#include "markets.hpp"

namespace qp {

/// Identifies an exchange. Multiple Markets map to one ExchangeId.
enum class ExchangeId : std::uint8_t {
    Binance = 0,
};

/// The exchange that hosts the given market.
constexpr ExchangeId exchange_of(Market m) noexcept {
    switch (m) {
        case Market::BinanceUsdm:
        case Market::BinanceCoinm:
        case Market::BinanceSpot:
            return ExchangeId::Binance;
    }
    return ExchangeId::Binance;
}

}  // namespace qp
