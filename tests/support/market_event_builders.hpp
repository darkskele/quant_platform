#pragma once
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "types.hpp"

namespace qp::test {

inline MarketEvent make_book_diff(std::uint16_t symbol, Timestamp ts, std::uint64_t first_seq,
                                  std::uint64_t seq, std::uint64_t prev_seq,
                                  std::vector<PriceLevel> bids   = {},
                                  std::vector<PriceLevel> asks   = {},
                                  std::uint16_t              market = 0,
                                  std::uint16_t              exchange = 0) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::BookDiff,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = ts};
    ev.payload = BookDiffEvent{
        .first_seq = first_seq,
        .seq       = seq,
        .prev_seq  = prev_seq,
        .levels    = std::make_shared<BookLevels>(BookLevels{std::move(bids), std::move(asks)}),
    };
    return ev;
}

inline MarketEvent make_book_snapshot(std::uint16_t symbol, Timestamp ts,
                                      std::vector<PriceLevel> bids   = {},
                                      std::vector<PriceLevel> asks   = {},
                                      std::uint16_t              market = 0,
                                      std::uint16_t              exchange = 0) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::BookSnapshot,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = ts};
    ev.payload = BookSnapshotEvent{
        .levels = std::make_shared<BookLevels>(BookLevels{std::move(bids), std::move(asks)}),
    };
    return ev;
}

inline MarketEvent make_trade(std::uint16_t symbol, Timestamp ts, Price price, Qty qty = 1.0,
                              Side side = Side::Buy, std::uint16_t market = 0,
                              std::uint16_t exchange = 0) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::Trade,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = ts};
    ev.payload = TradeEvent{.side = side, .price = price, .qty = qty};
    return ev;
}

inline MarketEvent make_funding(std::uint16_t symbol, Timestamp ts, double rate,
                                std::uint16_t market = 0, std::uint16_t exchange = 0) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::Funding,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = ts};
    ev.payload = FundingEvent{.funding_rate = rate};
    return ev;
}

inline MarketEvent make_kline(std::uint16_t symbol, Timestamp open_time, Timestamp close_time,
                              Price open, Price high, Price low, Price close, Qty volume = 0.0,
                              std::uint16_t market = 0, std::uint16_t exchange = 0) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::Kline,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = open_time};
    ev.payload = KlineEvent{.close_time = close_time,
                            .open       = open,
                            .high       = high,
                            .low        = low,
                            .close      = close,
                            .volume     = volume};
    return ev;
}

inline MarketEvent make_mark_price_kline(std::uint16_t symbol, Timestamp open_time, Price close,
                                         Timestamp close_time = 0, Price open = 0.0,
                                         Price high = 0.0, Price low = 0.0,
                                         std::uint16_t market = 0, std::uint16_t exchange = 0) {
    MarketEvent ev;
    ev.base    = {.kind     = EventKind::MarkPriceKline,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = open_time};
    ev.payload = MarkPriceKlineEvent{.close_time = close_time,
                                     .open       = open,
                                     .high       = high,
                                     .low        = low,
                                     .close      = close};
    return ev;
}

}  // namespace qp::test
