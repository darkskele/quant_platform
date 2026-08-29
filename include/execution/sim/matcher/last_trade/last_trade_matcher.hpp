#pragma once
#include <cstddef>
#include <unordered_map>
#include <variant>

#include "matcher.hpp"
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
/// recreated here if this map ignored it. Rejects with NoPriceAvailable if
/// no Trade has been seen yet for that (symbol, venue): an honest "can't
/// fill" beats a fabricated price.
///
/// unordered_map is a known-bad fit here vs. Portfolio's flat-array
/// discipline (D31/D44) — left as-is for now, revisit separately.
class LastTradeMatcher {
   public:
    void on_market_event(MarketEvent ev) {
        if (ev.kind == EventKind::Trade) last_price_[key(ev.symbol, ev.venue)] = ev.price;
    }

    std::variant<Fill, Reject> try_fill(Order o, Timestamp ts) {
        auto it = last_price_.find(key(o.symbol, o.venue));
        if (it == last_price_.end()) {
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

        Price price = it->second;
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
    // Combined (symbol, venue) key — VenueId is a uint8_t, so shifting
    // SymbolId left by 8 bits and OR-ing venue in loses nothing and needs
    // no custom hash/pair machinery, matching Portfolio's own flat-index
    // discipline (D31/D44) as closely as an unordered_map allows.
    static std::uint64_t key(SymbolId symbol, VenueId venue) noexcept {
        return (static_cast<std::uint64_t>(symbol) << 8) | venue;
    }

    std::unordered_map<std::uint64_t, Price> last_price_;
};

static_assert(Matcher<LastTradeMatcher>);

}  // namespace qp::execution::sim::matcher::last_trade
