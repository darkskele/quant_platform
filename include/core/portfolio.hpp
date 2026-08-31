#pragma once
#include <array>
#include <cassert>
#include <concepts>
#include <type_traits>
#include <variant>

#include "types.hpp"

namespace qp {

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
template <auto Counts>
class Portfolio {
   public:
    static constexpr std::size_t kNumVenues      = Counts.size();
    static constexpr std::size_t kMaxInstruments = [] {
        std::size_t total = 0;
        for (std::size_t c : Counts) total += c;
        return total;
    }();

    void apply_fill(const Fill& fill) noexcept {
        Qty delta = fill.side == Side::Buy ? fill.qty : -fill.qty;
        positions_[index(fill.symbol, fill.venue)] += delta;
        cash_ -= delta * fill.price + fill.fee;  // buying costs cash, fee always does
    }

    /// A no-op unless `event` actually holds a FundingEvent. Settles
    /// against the last-seen *official* mark price (funding_mark_price_,
    /// fed only by MarkPriceKlineEvent), never the general mark_price_
    /// valuation price below — that one can most-recently reflect a Trade
    /// or Kline instead, and settling funding against last-trade is
    /// exactly the manipulation the real mark price exists to prevent
    /// (see MarkPriceKlineEvent's own comment). `position(event.symbol,
    /// event.venue)` as it stands right now — call before letting a
    /// Strategy react to this same event, so a decision made *because of*
    /// this rate can't also be charged the payment it triggered.
    void apply_funding(const MarketEvent& event) noexcept {
        const auto* funding = std::get_if<FundingEvent>(&event);
        if (!funding) return;
        // Positive funding_rate: longs pay shorts — a positive (long)
        // position debits cash, a negative (short) one credits it.
        std::size_t i = index(funding->symbol, funding->venue);
        cash_ -= positions_[i] * funding_mark_price_[i] * funding->funding_rate;
    }

    /// Marks (symbol, venue) at whichever scalar price `event` actually
    /// carries — Trade's `price`, Kline's `close`, or MarkPriceKline's
    /// `close` (D46); BookDiff/BookSnapshot have no single scalar price and
    /// are ignored. Feeds equity(), which otherwise has no notion of
    /// unrealized PnL — cash() alone reflects realized trading cash flow,
    /// not what a held position is currently worth. MarkPriceKline also
    /// updates funding_mark_price_ (apply_funding's dedicated source) —
    /// deliberately separate from the general mark_price_ array above,
    /// even though this is the one case where both get the same value:
    /// keeping them as two arrays means a Trade/Kline arriving after the
    /// last MarkPriceKline can update valuation without silently also
    /// becoming what funding settles against. Starts at 0 (unmarked) like
    /// positions_/cash_: a position held before its first relevant event
    /// understates equity() until one arrives, same "no
    /// never-touched-vs-zero distinction" as the rest of this class.
    void apply_mark_price(const MarketEvent& event) noexcept {
        std::visit(
            [this](const auto& e) {
                using E = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<E, TradeEvent>) {
                    mark(e.symbol, e.venue, e.price);
                } else if constexpr (std::is_same_v<E, KlineEvent>) {
                    mark(e.symbol, e.venue, e.close);
                } else if constexpr (std::is_same_v<E, MarkPriceKlineEvent>) {
                    mark(e.symbol, e.venue, e.close);
                    mark_funding(e.symbol, e.venue, e.close);
                }
            },
            event);
    }

    /// No default for `venue` (D44, matching AlignmentRule/BinanceMarket's
    /// own "no default" precedent): (symbol, venue) together identify an
    /// instrument, symbol alone doesn't — a default would silently answer
    /// "venue 0" for a caller that forgot to think about which leg it means.
    Qty position(SymbolId symbol, VenueId venue) const noexcept {
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
        for (std::size_t i = 0; i < kMaxInstruments; ++i) total += positions_[i] * mark_price_[i];
        return total;
    }

    static constexpr std::size_t index(SymbolId symbol, VenueId venue) noexcept {
        assert(venue < kNumVenues);
        assert(symbol < Counts[venue]);
        return kPrefix[venue] + symbol;
    }

   private:
    static constexpr auto kPrefix = [] {
        std::array<std::size_t, kNumVenues> prefix{};
        std::size_t                         running = 0;
        for (std::size_t i = 0; i < kNumVenues; ++i) {
            prefix[i] = running;
            running += Counts[i];
        }
        return prefix;
    }();

    void mark(SymbolId symbol, VenueId venue, Price price) noexcept {
        mark_price_[index(symbol, venue)] = price;
    }

    void mark_funding(SymbolId symbol, VenueId venue, Price price) noexcept {
        funding_mark_price_[index(symbol, venue)] = price;
    }

    std::array<Qty, kMaxInstruments>   positions_{};
    std::array<Price, kMaxInstruments> mark_price_{};
    std::array<Price, kMaxInstruments> funding_mark_price_{};
    Notional                           cash_{0.0};
};

/// The seam Engine/Strategy/RiskGate depend on instead of the concrete
/// Portfolio (D27's "seam, not concrete adapter" discipline, extended to
/// account state) — describes exactly the calls made against it: Engine's
/// two write paths, plus position()/equity() for whoever only reads.
/// Strategy/RiskGate hold `const Book&` (read-only, enforced by
/// constness — no separate read-only wrapper type needed, that's what
/// this concept plus `const` already give for free); Engine holds the
/// mutable `Book&` that actually calls apply_fill/apply_funding/
/// apply_mark_price. A plain Portfolio satisfies all of this trivially.
/// The point isn't swapping implementations (there's still exactly one) —
/// it's that Engine holds Book by reference, not by value, so a
/// composition root can point several Engines/RiskGates/Strategies at one
/// shared instance (one account, several strategies, one true equity/
/// position book) without any of them caring how that sharing is
/// implemented underneath. kMaxSymbols/kMaxVenues are required too —
/// BasicRiskGate<Book> already reaches for Book::kMaxSymbols/kMaxVenues to
/// size its own orders_ pool, so that dependency belongs in the concept's
/// contract, not left as an implicit assumption true only because
/// Portfolio is still the one Book that exists.
template <class T>
concept PortfolioLike =
    requires(T p, const Fill& fill, const MarketEvent& event, SymbolId symbol, VenueId venue) {
        { T::kMaxInstruments } -> std::convertible_to<std::size_t>;
        { T::index(symbol, venue) } -> std::same_as<std::size_t>;
        { p.apply_fill(fill) } -> std::same_as<void>;
        { p.apply_funding(event) } -> std::same_as<void>;
        { p.apply_mark_price(event) } -> std::same_as<void>;
        { p.position(symbol, venue) } -> std::same_as<Qty>;
        { p.equity() } -> std::same_as<Notional>;
    };

namespace detail {
// One venue, one symbol — not a real universe, just enough for a
// concrete Portfolio to plug into concept self-checks here and in
// sibling headers (basic_risk_gate.hpp, last_trade_matcher.hpp,
// funding_carry_strategy.hpp) without depending on any real venue data.
inline constexpr std::array<std::size_t, 1> kTrivialCounts{1};
}  // namespace detail

static_assert(PortfolioLike<Portfolio<detail::kTrivialCounts>>);

}  // namespace qp
