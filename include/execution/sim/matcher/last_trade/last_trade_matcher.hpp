#pragma once
#include <array>
#include <cstddef>
#include <variant>

#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::last_trade {

/// SimExecution's first Matcher: no order book, no slippage, no
/// partials — fills a market order fully, instantly, at the last-seen
/// price for its (symbol, venue) (funding carry, the first strategy
/// family, needs no depth — docs/strategy.md). Priced off either a Trade
/// (its `price`) or a Kline (its `close`) — historical venues like
/// binance_historical never emit Trade at all, only Kline/MarkPriceKline/
/// Funding (bin_hist_parser_policy.hpp), so Trade-only pricing would leave
/// this matcher permanently unable to fill against that data. MarkPriceKline
/// deliberately excluded: it's the official mark (Portfolio's
/// funding_mark_price_, apply_funding's dedicated source), not a traded
/// price — filling against it would let a strategy transact at a price
/// nothing actually traded at. Keyed by (symbol, venue), not symbol alone
/// (D44): two venues intern the same underlying instrument to the same
/// SymbolId, so keying on symbol alone would let a spot bar's price
/// silently overwrite a perp bar's (or vice versa) — exactly the collision
/// MarketEvent::venue (D43) exists to prevent, recreated here if this array
/// ignored it. Rejects with NoPriceAvailable if neither has been seen yet
/// for that (symbol, venue): an honest "can't fill" beats a fabricated
/// price — Price{0} (never a real price) is the sentinel, same
/// direct-indexed discipline as Portfolio (D31/D44), no hashing, no heap.
template <qp::PortfolioLike Book>
class LastTradeMatcher {
   public:
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

        // Binance USD-M futures VIP0 taker rate, from general knowledge —
        // not pinned to a live fee schedule. Confirm/refresh before
        // trusting this against real capital; not configurable yet, no
        // second rate exists to configure between.
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
    // kMaxVenues (8) * sizeof(Price) (8) == 64: one symbol's whole venue row
    // is exactly one cache line, but only if the array itself starts on a
    // line boundary — without alignas(64), std::array<Price,...> is only
    // 8-byte aligned, so a row can straddle two lines depending on where
    // this object lands; a live strategy's on_market_event/try_fill
    // traffic for one symbol's spot+futures legs (D44) then costs up to 2
    // cache misses instead of 1.
    alignas(64) std::array<Price, Book::kMaxInstruments> last_price_{};
};

static_assert(Matcher<LastTradeMatcher<Portfolio<qp::detail::kTrivialCounts>>>);

}  // namespace qp::execution::sim::matcher::last_trade
