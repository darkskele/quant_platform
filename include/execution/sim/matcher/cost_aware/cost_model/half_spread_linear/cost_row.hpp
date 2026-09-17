#pragma once
#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear {

struct CostRow {
    std::uint16_t exchange{};
    std::uint16_t market{};
    std::uint16_t symbol{};
    Timestamp     week_start_ns{};
    double        half_spread_bps{};
    double        impact_bps_per_unit{};
    double        taker_fee_bps{};
};

}  // namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear
