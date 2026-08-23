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

    Qty      position(SymbolId symbol) const noexcept;
    Notional cash() const noexcept;

   private:
    const Portfolio* portfolio_;
};

/// The feedback hub: net position per symbol + cash balance, fed by two
/// write paths (Engine-only): apply_fill (a trade) and apply_funding (a
/// Funding-kind event settling against whatever position is currently
/// held). Fixed-size, direct-indexed by SymbolId — SymbolId is already a
/// dense id assigned by the run's own SymbolTable::intern() (core/types.hpp:
/// "index into venue symbol table"), not a hash, so direct array indexing
/// is the correct structure here, not a workaround (D31): no allocation, no
/// hashing, on either write path. kMaxSymbols is sized for a solo retail
/// portfolio (funding carry majors + stat-arb pairs, docs/strategy.md) —
/// the run's own subscribed symbol set, not Binance's full catalog; bump it
/// if a real strategy set needs more.
class Portfolio {
   public:
    static constexpr std::size_t kMaxSymbols = 64;

    void apply_fill(const Fill& fill) noexcept {
        assert(fill.symbol < kMaxSymbols);
        Qty delta = fill.side == Side::Buy ? fill.qty : -fill.qty;
        positions_[fill.symbol] += delta;
        cash_ -= delta * fill.price + fill.fee;  // buying costs cash, fee always does
    }

    /// `event.kind` must be `Funding`. Settles against `position(event.symbol)`
    /// as it stands right now — call before letting a Strategy react to this
    /// same event, so a decision made *because of* this rate can't also be
    /// charged the payment it triggered.
    void apply_funding(const MarketEvent& event) noexcept {
        assert(event.symbol < kMaxSymbols);
        // Positive funding_rate: longs pay shorts — a positive (long)
        // position debits cash, a negative (short) one credits it.
        cash_ -= positions_[event.symbol] * event.mark_price * event.funding_rate;
    }

    Qty position(SymbolId symbol) const noexcept {
        assert(symbol < kMaxSymbols);
        return positions_[symbol];
    }

    Notional cash() const noexcept { return cash_; }

    StateView view() const noexcept { return StateView{*this}; }

   private:
    std::array<Qty, kMaxSymbols> positions_{};
    Notional                     cash_{0.0};
};

inline Qty StateView::position(SymbolId symbol) const noexcept {
    return portfolio_->position(symbol);
}

inline Notional StateView::cash() const noexcept { return portfolio_->cash(); }

}  // namespace qp
