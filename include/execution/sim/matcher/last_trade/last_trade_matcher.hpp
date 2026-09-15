#pragma once
#include <cstddef>
#include <variant>
#include <vector>

#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::last_trade {

/// Fills a market order fully, instantly, at the last-seen price for its
/// (symbol, market).
class LastTradeMatcher {
   public:
    explicit LastTradeMatcher(const Portfolio& book)
        : book_{&book}, last_price_(book.max_instruments(), 0.0) {}

    // Priced off a Trade or a Kline close.
    void on_market_event(const MarketEvent& ev) {
        if (const auto* trade = std::get_if<TradeEvent>(&ev)) {
            last_price_[book_->index(trade->base.symbol, trade->base.market)] = trade->price;
        } else if (const auto* kline = std::get_if<KlineEvent>(&ev)) {
            last_price_[book_->index(kline->base.symbol, kline->base.market)] = kline->close;
        }
    }

    std::variant<Fill, Reject> try_fill(Order o, Timestamp ts) {
        Price price = last_price_[book_->index(o.symbol, o.market)];
        if (price == 0.0) {
            return Reject{
                .order_id = o.id,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoPriceAvailable,
                .market   = o.market,
                .ts       = ts,
            };
        }

        // Binance USD-M futures VIP0 taker rate, from general knowledge,
        // not pinned to a live fee schedule.
        // @todo: Make this configurable and tuneable.
        constexpr Notional kTakerFeeRate = 0.0004;

        return Fill{
            .order_id = o.id,
            .symbol   = o.symbol,
            .side     = o.side,
            .market   = o.market,
            .ts       = ts,
            .price    = price,
            .qty      = o.qty,
            .fee      = price * o.qty * kTakerFeeRate,
        };
    }

   private:
    const Portfolio*   book_;
    std::vector<Price> last_price_;
};

static_assert(Matcher<LastTradeMatcher>);

}  // namespace qp::execution::sim::matcher::last_trade
