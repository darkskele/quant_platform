#pragma once
#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

#include "cost_model/cost_model.hpp"
#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware {

template <cost_model::CostModel CM>
class CostAwareMatcher {
   public:
    CostAwareMatcher(const Portfolio& book, CM cost) noexcept
        : book_{&book}, cost_{std::move(cost)}, last_price_(book.max_instruments(), 0.0) {}

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
        Price ref = last_price_[book_->index(o.exchange, o.market, o.symbol)];
        if (ref == 0.0) {
            return Reject{
                .order_id = o.id,
                .exchange = o.exchange,
                .market   = o.market,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoPriceAvailable,
                .ts       = ts,
            };
        }

        auto pricing = cost_.price(o, ref, ts);
        if (!pricing) {
            return Reject{
                .order_id = o.id,
                .exchange = o.exchange,
                .market   = o.market,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoCostAvailable,
                .ts       = ts,
            };
        }
        return Fill{
            .order_id = o.id,
            .exchange = o.exchange,
            .market   = o.market,
            .symbol   = o.symbol,
            .side     = o.side,
            .ts       = ts,
            .price    = pricing->fill_price,
            .qty      = o.qty,
            .fee      = pricing->fee,
        };
    }

   private:
    const Portfolio*   book_;
    CM                 cost_;
    std::vector<Price> last_price_;
};

}  // namespace qp::execution::sim::matcher::cost_aware
