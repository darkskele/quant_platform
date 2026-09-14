#pragma once
#include <cstdint>

#include "markets.hpp"

namespace qp {

/// Identifies an exchange. Multiple MarketIds map to one ExchangeId.
enum class ExchangeId : std::uint8_t {
    Binance = 0,
};

/// The exchange that hosts the given market.
constexpr ExchangeId exchange_of(MarketId m) noexcept {
    switch (m) {
        case markets::BinanceUsdm:
        case markets::BinanceCoinm:
        case markets::BinanceSpot:
            return ExchangeId::Binance;
    }
    return ExchangeId::Binance;
}

}  // namespace qp
