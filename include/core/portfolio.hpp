#pragma once
#include <array>
#include <cassert>
#include <concepts>

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
    Notional equity() const noexcept;

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

    /// Marks (symbol, venue) at whichever scalar price `event` actually
    /// carries — Trade's `price` or Funding's `mark_price` (D46); BookDiff/
    /// BookSnapshot have no single scalar price and are ignored. Feeds
    /// equity(), which otherwise has no notion of unrealized PnL — cash()
    /// alone reflects realized trading cash flow, not what a held position
    /// is currently worth. Starts at 0 (unmarked) like positions_/cash_: a
    /// position held before its first Trade/Funding event understates
    /// equity() until one arrives, same "no never-touched-vs-zero
    /// distinction" as the rest of this class.
    void apply_mark_price(const MarketEvent& event) noexcept {
        assert(event.symbol < kMaxSymbols);
        assert(event.venue < kMaxVenues);
        if (event.kind == EventKind::Trade) {
            mark_price_[index(event.symbol, event.venue)] = event.price;
        } else if (event.kind == EventKind::Funding) {
            mark_price_[index(event.symbol, event.venue)] = event.mark_price;
        }
    }

    Qty position(SymbolId symbol, VenueId venue) const noexcept {
        assert(symbol < kMaxSymbols);
        assert(venue < kMaxVenues);
        return positions_[index(symbol, venue)];
    }

    Notional cash() const noexcept { return cash_; }

    /// cash() plus every position's mark-to-market value (D46) — the actual
    /// figure a drawdown/kill-switch RiskGate needs; cash() alone treats
    /// "bought an asset" indistinguishably from "lost money". O(kMaxSymbols
    /// * kMaxVenues) linear scan, not tracked incrementally: called at most
    /// once per Engine::step() (RiskGate::on_tick), and 512 flat-array
    /// entries is cheap next to that cadence — no reason to pay bookkeeping
    /// cost on every apply_fill/apply_mark_price for a value read this
    /// rarely.
    Notional equity() const noexcept {
        Notional total = cash_;
        for (std::size_t i = 0; i < kMaxSymbols * kMaxVenues; ++i)
            total += positions_[i] * mark_price_[i];
        return total;
    }

    StateView view() const noexcept { return StateView{*this}; }

   private:
    static constexpr std::size_t index(SymbolId symbol, VenueId venue) noexcept {
        return static_cast<std::size_t>(symbol) * kMaxVenues + venue;
    }

    std::array<Qty, kMaxSymbols * kMaxVenues>   positions_{};
    std::array<Price, kMaxSymbols * kMaxVenues> mark_price_{};
    Notional                                    cash_{0.0};
};

inline Qty StateView::position(SymbolId symbol, VenueId venue) const noexcept {
    return portfolio_->position(symbol, venue);
}

inline Notional StateView::cash() const noexcept { return portfolio_->cash(); }

inline Notional StateView::equity() const noexcept { return portfolio_->equity(); }

/// The seam Engine (and any RiskGate that needs live account state) depend
/// on instead of the concrete Portfolio — same "seam, not concrete
/// adapter" discipline as every other Engine dependency (D27), extended to
/// account state. Describes exactly Engine's own write/read calls, plus
/// position()/equity() directly (not just view()): a RiskGate that's
/// already templated on Book — unlike Strategy, which stays fixed on
/// StateView so it never needs to know the concrete Book type — has no
/// reason to route through a StateView it's only going to immediately call
/// through, same destination either way (see the StateView:: forwarders
/// just above). view() stays required for Engine's own sake: Strategy's
/// concept is fixed on StateView, so Engine needs a way to produce one.
/// kMaxSymbols/kMaxVenues are required too — BasicRiskGate<Book> already
/// reaches for Book::kMaxSymbols/kMaxVenues to size its own orders_ pool,
/// so that dependency belongs in the concept's contract, not left as an
/// implicit assumption true only because Portfolio is still the one Book
/// that exists. A plain Portfolio satisfies all of this trivially. The
/// point isn't swapping implementations (there's still exactly one) —
/// it's that Engine holds Book by reference, not by value, so a
/// composition root can point several Engines/RiskGates at one shared
/// instance (one account, several strategies, one true equity/position
/// book) without Engine's own code caring how that sharing is implemented
/// underneath.
template <class T>
concept PortfolioLike =
    requires(T p, const Fill& fill, const MarketEvent& event, SymbolId symbol, VenueId venue) {
        { T::kMaxSymbols } -> std::convertible_to<std::size_t>;
        { T::kMaxVenues } -> std::convertible_to<std::size_t>;
        { p.apply_fill(fill) } -> std::same_as<void>;
        { p.apply_funding(event) } -> std::same_as<void>;
        { p.apply_mark_price(event) } -> std::same_as<void>;
        { p.position(symbol, venue) } -> std::same_as<Qty>;
        { p.equity() } -> std::same_as<Notional>;
        { p.view() } -> std::same_as<StateView>;
    };

static_assert(PortfolioLike<Portfolio>);

}  // namespace qp
