#pragma once
#include "types.hpp"

namespace qp::test {

inline Fill make_fill(std::uint16_t symbol, Side side, Qty qty, Price price = 100.0,
                      OrderId order_id = 1, Timestamp ts = 0, Notional fee = 0.0,
                      std::uint16_t market = 0, std::uint16_t exchange = 0) {
    Fill fill;
    fill.order_id = order_id;
    fill.exchange = exchange;
    fill.market   = market;
    fill.symbol   = symbol;
    fill.side     = side;
    fill.ts       = ts;
    fill.price    = price;
    fill.qty      = qty;
    fill.fee      = fee;
    return fill;
}

}  // namespace qp::test
