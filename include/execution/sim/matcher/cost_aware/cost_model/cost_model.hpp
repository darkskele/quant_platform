#pragma once
#include <optional>

#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware::cost_model {

/// Fill price and fee for one order.
struct FillPricing {
    Price    fill_price{};
    Notional fee{};
};

/// Prices a fill and its fee for an Order at a reference price and time.
template <class T>
concept CostModel = requires(const T c, const Order& o, Price ref, Timestamp ts) {
    { c.price(o, ref, ts) } -> std::same_as<std::optional<FillPricing>>;
};

}  // namespace qp::execution::sim::matcher::cost_aware::cost_model
