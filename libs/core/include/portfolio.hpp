#pragma once
#include <array>
#include <cassert>

#include "types.hpp"

namespace qp {

class Portfolio;

/// Read-only view onto a Portfolio's live state — the read half of the
/// read/write split (architecture-principles.md seam 6). Strategy/RiskGate
/// only ever see this, never a mutable Portfolio&. Cheap, non-owning (one
/// pointer) — pass by value like a span. A live window, not a snapshot: it
/// reflects apply_fill() calls made after construction.
class StateView {
   public:
    explicit StateView(const Portfolio& portfolio) noexcept : portfolio_{&portfolio} {}

    /// No default for `venue` (D44, matching AlignmentRule/BinanceMarket's
    /// own "no default" precedent): (symbol, venue) together identify an
    /// instrument, symbol alone doesn't — a default would silently answer
    /// "venue 0" for a caller that forgot to think about which leg it means.
    Qty      position(SymbolId symbol, VenueId venue) const noexcept;
    Notional cash() const noexcept;

   private:
    const Portfolio* portfolio_;
};

/// The feedback hub: net position per (symbol, venue) + cash balance, fed
/// by two write paths (Engine-only): apply_fill (a trade) and apply_funding
/// (a Funding-kind event settling against whatever position is currently
/// held). Fixed-size, direct-indexed by (SymbolId, VenueId) — both are
/// already dense ids (SymbolId from the run's own SymbolTable::intern(),
/// VenueId from whichever FanoutSink::record<I> stamped it, D43/D44), not
/// hashes, so direct array indexing is the correct structure here, not a
/// workaround (D31, extended D44): no allocation, no hashing, on either
/// write path. kMaxSymbols/kMaxVenues are sized for a solo retail portfolio
/// (funding carry majors + stat-arb pairs, docs/strategy.md; a small
/// handful of venues, not Binance's full catalog) — bump either if a real
/// strategy/wiring needs more.
class Portfolio {
   public:
    static constexpr std::size_t kMaxSymbols = 64;
    static constexpr std::size_t kMaxVenues  = 8;

    void apply_fill(const Fill& fill) noexcept {
        assert(fill.symbol < kMaxSymbols);
        assert(fill.venue < kMaxVenues);
        Qty delta = fill.side == Side::Buy ? fill.qty : -fill.qty;
        positions_[index(fill.symbol, fill.venue)] += delta;
        cash_ -= delta * fill.price + fill.fee;  // buying costs cash, fee always does
    }

    /// `event.kind` must be `Funding`. Settles against
    /// `position(event.symbol, event.venue)` as it stands right now — call
    /// before letting a Strategy react to this same event, so a decision
    /// made *because of* this rate can't also be charged the payment it
    /// triggered.
    void apply_funding(const MarketEvent& event) noexcept {
        assert(event.symbol < kMaxSymbols);
        assert(event.venue < kMaxVenues);
        // Positive funding_rate: longs pay shorts — a positive (long)
        // position debits cash, a negative (short) one credits it.
        cash_ -=
            positions_[index(event.symbol, event.venue)] * event.mark_price * event.funding_rate;
    }

    Qty position(SymbolId symbol, VenueId venue) const noexcept {
        assert(symbol < kMaxSymbols);
        assert(venue < kMaxVenues);
        return positions_[index(symbol, venue)];
    }

    Notional cash() const noexcept { return cash_; }

    StateView view() const noexcept { return StateView{*this}; }

   private:
    static constexpr std::size_t index(SymbolId symbol, VenueId venue) noexcept {
        return static_cast<std::size_t>(symbol) * kMaxVenues + venue;
    }

    std::array<Qty, kMaxSymbols * kMaxVenues> positions_{};
    Notional                                  cash_{0.0};
};

inline Qty StateView::position(SymbolId symbol, VenueId venue) const noexcept {
    return portfolio_->position(symbol, venue);
}

inline Notional StateView::cash() const noexcept { return portfolio_->cash(); }

}  // namespace qp
