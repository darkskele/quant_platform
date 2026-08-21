#pragma once
#include <unordered_map>
#include <variant>

#include "matcher.hpp"
#include "types.hpp"

namespace qp::execution {

/// SimExecution's first Matcher: no order book, no slippage, no
/// partials — fills a market order fully, instantly, at the last-seen
/// Trade price for its symbol (funding carry, the first strategy family,
/// needs no depth — docs/strategy.md). Rejects with NoPriceAvailable if no
/// Trade has been seen yet for that symbol: an honest "can't fill" beats a
/// fabricated price.
class LastTradeMatcher {
   public:
    void on_market_event(MarketEvent ev) {
        if (ev.kind == EventKind::Trade) last_price_[ev.symbol] = ev.price;
    }

    std::variant<Fill, Reject> try_fill(Order o, Timestamp ts) {
        auto it = last_price_.find(o.symbol);
        if (it == last_price_.end()) {
            return Reject{
                .order_id = o.id,
                .symbol   = o.symbol,
                .reason   = RejectReason::NoPriceAvailable,
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
            .ts       = ts,
            .price    = price,
            .qty      = o.qty,
            .fee      = price * o.qty * kTakerFeeRate,
        };
    }

   private:
    std::unordered_map<SymbolId, Price> last_price_;
};

}  // namespace qp::execution
