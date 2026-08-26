#pragma once
#include <cstdint>
#include <utility>
#include <vector>

#include "types.hpp"

namespace qp::test {

inline MarketEvent make_book_diff(SymbolId symbol, Timestamp ts, std::uint64_t first_seq,
                                  std::uint64_t seq, std::uint64_t prev_seq,
                                  std::vector<PriceLevel> bids = {},
                                  std::vector<PriceLevel> asks = {}, VenueId venue = 0) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.ts        = ts;
    ev.first_seq = first_seq;
    ev.seq       = seq;
    ev.prev_seq  = prev_seq;
    ev.symbol    = symbol;
    ev.venue     = venue;
    ev.bids      = std::move(bids);
    ev.asks      = std::move(asks);
    return ev;
}

inline MarketEvent make_trade(SymbolId symbol, Timestamp ts, Price price, Qty qty = 1.0,
                              Side side = Side::Buy, VenueId venue = 0) {
    MarketEvent ev;
    ev.kind   = EventKind::Trade;
    ev.ts     = ts;
    ev.symbol = symbol;
    ev.price  = price;
    ev.qty    = qty;
    ev.side   = side;
    ev.venue  = venue;
    return ev;
}

inline MarketEvent make_funding(SymbolId symbol, Timestamp ts, double rate, Price mark_price = 0.0,
                                VenueId venue = 0) {
    MarketEvent ev;
    ev.kind         = EventKind::Funding;
    ev.ts           = ts;
    ev.symbol       = symbol;
    ev.funding_rate = rate;
    ev.mark_price   = mark_price;
    ev.venue        = venue;
    return ev;
}

}  // namespace qp::test
