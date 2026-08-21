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

    Qty position(SymbolId symbol) const noexcept;

   private:
    const Portfolio* portfolio_;
};

/// The feedback hub: net position per symbol, fed by the write path
/// (apply_fill, called only by Engine after a Fill outcome). Fixed-size,
/// direct-indexed by SymbolId — SymbolId is already a dense id assigned by
/// the run's own SymbolTable::intern() (core/types.hpp: "index into venue
/// symbol table"), not a hash, so direct array indexing is the correct
/// structure here, not a workaround (D31): no allocation, no hashing, on
/// apply_fill/position. kMaxSymbols is sized for a solo retail portfolio
/// (funding carry majors + stat-arb pairs, docs/strategy.md) — the run's
/// own subscribed symbol set, not Binance's full catalog; bump it if a
/// real strategy set needs more. No PnL/open-order tracking yet — see
/// docs/decisions.md D28.
class Portfolio {
   public:
    static constexpr std::size_t kMaxSymbols = 64;

    void apply_fill(const Fill& fill) noexcept {
        assert(fill.symbol < kMaxSymbols);
        positions_[fill.symbol] += fill.side == Side::Buy ? fill.qty : -fill.qty;
    }

    Qty position(SymbolId symbol) const noexcept {
        assert(symbol < kMaxSymbols);
        return positions_[symbol];
    }

    StateView view() const noexcept { return StateView{*this}; }

   private:
    std::array<Qty, kMaxSymbols> positions_{};
};

inline Qty StateView::position(SymbolId symbol) const noexcept {
    return portfolio_->position(symbol);
}

}  // namespace qp
