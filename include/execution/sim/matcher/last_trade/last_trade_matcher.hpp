#pragma once
#include <cstddef>
#include <variant>
#include <vector>

#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::last_trade {

class LastTradeMatcher {
   public:
    explicit LastTradeMatcher(const Portfolio& book)
        : book_{&book}, last_price_(book.max_instruments(), 0.0) {}

    void on_market_event(const MarketEvent& ev) {
        if (const auto* trade = std::get_if<TradeEvent>(&ev.payload)) {
            last_price_[book_->index(ev.base.exchange, ev.base.market, ev.base.symbol)] =
                trade->price;
        } else if (const auto* kline = std::get_if<KlineEvent>(&ev.payload)) {
            last_price_[book_->index(ev.base.exchange, ev.base.market, ev.base.symbol)] =
                kline->close;
        }
    }

    std::variant<Fill, Reject> try_fill(Order o, Timestamp ts) {
        Price price = last_price_[book_->index(o.exchange, o.market, o.symbol)];
        if (price == 0.0) {
            return Reject{
                .order_id = o.id,
                .exchange = o.exchange,
                .market   = o.market,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoPriceAvailable,
                .ts       = ts,
            };
        }

        constexpr Notional kTakerFeeRate = 0.0004;

        return Fill{
            .order_id = o.id,
            .exchange = o.exchange,
            .market   = o.market,
            .symbol   = o.symbol,
            .side     = o.side,
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
