#pragma once
#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear {

/// One row of the honest cost table.
struct CostRow {
    SymbolId  symbol{};
    Market    market{};
    Timestamp week_start_ns{};  ///< Nanoseconds since epoch, midnight UTC of the week's ISO Monday.
    double    half_spread_bps{};
    double    impact_bps_per_unit{};
    double    taker_fee_bps{};
};

}  // namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear
