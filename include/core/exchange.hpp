#pragma once
#include <cstdint>

namespace qp {

/// Identifies an exchange at config/subscription level. Each exchange source
/// binds to one of these. 
enum class ExchangeId : std::uint8_t {
    Binance = 0,
};

}  // namespace qp
