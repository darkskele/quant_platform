#pragma once
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "types.hpp"

namespace qp::test {

inline MarketEvent make_book_diff(SymbolId symbol, Timestamp ts, std::uint64_t first_seq,
                                  std::uint64_t seq, std::uint64_t prev_seq,
                                  std::vector<PriceLevel> bids = {},
                                  std::vector<PriceLevel> asks = {}, VenueId venue = 0) {
    BookDiffEvent ev;
    ev.ts        = ts;
    ev.first_seq = first_seq;
    ev.seq       = seq;
    ev.prev_seq  = prev_seq;
    ev.symbol    = symbol;
    ev.venue     = venue;
    ev.levels    = std::make_shared<BookLevels>(BookLevels{std::move(bids), std::move(asks)});
    return ev;
}

inline MarketEvent make_book_snapshot(SymbolId symbol, Timestamp ts,
                                      std::vector<PriceLevel> bids = {},
                                      std::vector<PriceLevel> asks = {}, VenueId venue = 0) {
    BookSnapshotEvent ev;
    ev.ts     = ts;
    ev.symbol = symbol;
    ev.venue  = venue;
    ev.levels = std::make_shared<BookLevels>(BookLevels{std::move(bids), std::move(asks)});
    return ev;
}

inline MarketEvent make_trade(SymbolId symbol, Timestamp ts, Price price, Qty qty = 1.0,
                              Side side = Side::Buy, VenueId venue = 0) {
    TradeEvent ev;
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
    FundingEvent ev;
    ev.ts           = ts;
    ev.symbol       = symbol;
    ev.funding_rate = rate;
    ev.mark_price   = mark_price;
    ev.venue        = venue;
    return ev;
}

inline MarketEvent make_kline(SymbolId symbol, Timestamp open_time, Timestamp close_time,
                              Price open, Price high, Price low, Price close, Qty volume = 0.0,
                              VenueId venue = 0) {
    KlineEvent ev;
    ev.ts         = open_time;
    ev.close_time = close_time;
    ev.symbol     = symbol;
    ev.open       = open;
    ev.high       = high;
    ev.low        = low;
    ev.close      = close;
    ev.volume     = volume;
    ev.venue      = venue;
    return ev;
}

}  // namespace qp::test
