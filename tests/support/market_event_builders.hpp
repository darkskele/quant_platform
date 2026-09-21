#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

#include "types.hpp"

namespace qp::test {

/// A bookDepth sample with every band populated, so a consumer that should
/// ignore it has real bands to ignore.
inline MarketEvent make_book_depth(std::uint16_t symbol, Timestamp ts, std::uint16_t market = 0,
                                   std::uint16_t exchange = 0) {
    BookDepthBands bands{};
    for (std::size_t k = 0; k < bands.bids.size(); ++k) {
        const double scale = static_cast<double>(k + 1);
        bands.bids[k]      = DepthBand{.depth = 10.0 * scale, .notional = 1000.0 * scale};
        bands.asks[k]      = DepthBand{.depth = 12.0 * scale, .notional = 1200.0 * scale};
    }

    MarketEvent ev;
    ev.base    = {.kind     = EventKind::BookDepth,
                  .exchange = exchange,
                  .market   = market,
                  .symbol   = symbol,
                  .ts       = ts};
    ev.payload = BookDepthEvent{.bands = std::make_shared<const BookDepthBands>(bands)};
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
    ev.payload = TradeEvent{.price = price, .qty = qty, .side = side};
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
    ev.payload = MarkPriceKlineEvent{
        .close_time = close_time, .open = open, .high = high, .low = low, .close = close};
    return ev;
}

}  // namespace qp::test
