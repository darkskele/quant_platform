#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <variant>

#include "matcher.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::last_trade {

/// SimExecution's first Matcher: no order book, no slippage, no
/// partials — fills a market order fully, instantly, at the last-seen
/// Trade price for its (symbol, venue) (funding carry, the first strategy
/// family, needs no depth — docs/strategy.md). Keyed by (symbol, venue),
/// not symbol alone (D44): two venues intern the same underlying
/// instrument to the same SymbolId, so keying on symbol alone would let a
/// spot trade's price silently overwrite a perp trade's (or vice versa) —
/// exactly the collision MarketEvent::venue (D43) exists to prevent,
/// recreated here if this array ignored it. Rejects with NoPriceAvailable
/// if no Trade has been seen yet for that (symbol, venue): an honest
/// "can't fill" beats a fabricated price — Price{0} (never a real trade
/// price) is the sentinel, same direct-indexed discipline as Portfolio
/// (D31/D44), no hashing, no heap.
class LastTradeMatcher {
   public:
    void on_market_event(MarketEvent ev) {
        if (ev.kind != EventKind::Trade) return;
        assert(ev.symbol < Portfolio::kMaxSymbols);
        assert(ev.venue < Portfolio::kMaxVenues);
        last_price_[index(ev.symbol, ev.venue)] = ev.price;
    }

    std::variant<Fill, Reject> try_fill(Order o, Timestamp ts) {
        assert(o.symbol < Portfolio::kMaxSymbols);
        assert(o.venue < Portfolio::kMaxVenues);
        Price price = last_price_[index(o.symbol, o.venue)];
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
    static constexpr std::size_t index(SymbolId symbol, VenueId venue) noexcept {
        return static_cast<std::size_t>(symbol) * Portfolio::kMaxVenues + venue;
    }

    // kMaxVenues (8) * sizeof(Price) (8) == 64: one symbol's whole venue row
    // is exactly one cache line, but only if the array itself starts on a
    // line boundary — without alignas(64), std::array<Price,...> is only
    // 8-byte aligned, so a row can straddle two lines depending on where
    // this object lands; a live strategy's on_market_event/try_fill
    // traffic for one symbol's spot+futures legs (D44) then costs up to 2
    // cache misses instead of 1.
    alignas(64) std::array<Price, Portfolio::kMaxSymbols * Portfolio::kMaxVenues> last_price_{};
};

static_assert(Matcher<LastTradeMatcher>);

}  // namespace qp::execution::sim::matcher::last_trade
