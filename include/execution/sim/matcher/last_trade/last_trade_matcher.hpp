#pragma once
#include <array>
#include <cstddef>
#include <variant>

#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::last_trade {

/// Fills a market order fully, instantly, at the last-seen price for its
/// (symbol, venue).
template <qp::PortfolioLike Book>
class LastTradeMatcher {
   public:
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
        Price price = last_price_[Book::index(o.symbol, o.venue)];
        if (price == 0.0) {
            return Reject{
                .order_id = o.id,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoPriceAvailable,
                .venue    = o.venue,
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
            .venue    = o.venue,
            .ts       = ts,
            .price    = price,
            .qty      = o.qty,
            .fee      = price * o.qty * kTakerFeeRate,
        };
    }

   private:
    // alignas(64): one symbol's whole venue row fits one cache line.
    alignas(64) std::array<Price, Book::kMaxInstruments> last_price_{};
};

static_assert(Matcher<LastTradeMatcher<Portfolio<qp::detail::kTrivialCounts>>>);

}  // namespace qp::execution::sim::matcher::last_trade
