#pragma once
#include <array>
#include <cstddef>
#include <utility>
#include <variant>

#include "cost_model/cost_model.hpp"
#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware {

/// Fills a market order fully at the last-seen price, adjusted by a CostModel.
/// A miss on either the reference price or the cost row is a Reject.
/// @tparam Book Portfolio-like: provides kMaxInstruments and index().
/// @tparam CM   CostModel-conforming.
template <qp::PortfolioLike Book, cost_model::CostModel CM>
class CostAwareMatcher {
   public:
    explicit CostAwareMatcher(CM cost) noexcept : cost_(std::move(cost)) {}

    // Priced off a Trade or a Kline close: historical data never emits
    // Trade at all, only Kline/Funding.
    void on_market_event(const MarketEvent& ev) {
        if (const auto* trade = std::get_if<TradeEvent>(&ev)) {
            last_price_[Book::index(trade->symbol, trade->venue)] = trade->price;
        } else if (const auto* kline = std::get_if<KlineEvent>(&ev)) {
            last_price_[Book::index(kline->symbol, kline->venue)] = kline->close;
        }
    }

    std::variant<Fill, Reject> try_fill(Order o, Timestamp ts) {
        Price ref = last_price_[Book::index(o.symbol, o.venue)];
        if (ref == 0.0) {
            return Reject{
                .order_id = o.id,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoPriceAvailable,
                .venue    = o.venue,
                .ts       = ts,
            };
        }

        auto pricing = cost_.price(o, ref, ts);
        if (!pricing) {
            return Reject{
                .order_id = o.id,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoCostAvailable,
                .venue    = o.venue,
                .ts       = ts,
            };
        }
        return Fill{
            .order_id = o.id,
            .symbol   = o.symbol,
            .side     = o.side,
            .venue    = o.venue,
            .ts       = ts,
            .price    = pricing->fill_price,
            .qty      = o.qty,
            .fee      = pricing->fee,
        };
    }

   private:
    CM cost_;
    // one symbol's whole venue row fits one cache line.
    alignas(64) std::array<Price, Book::kMaxInstruments> last_price_{};
};

}  // namespace qp::execution::sim::matcher::cost_aware
