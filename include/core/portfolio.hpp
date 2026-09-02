#pragma once
#include <array>
#include <cassert>
#include <concepts>
#include <type_traits>
#include <variant>

#include "types.hpp"

namespace qp {

/// Net position per (symbol, venue) plus cash balance.
/// Direct-indexed by (SymbolId, VenueId).
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

    /// No-op unless `event` holds a FundingEvent. Settles against the
    /// official mark price (funding_mark_price_, starts at 0 until an
    /// apply_mark_price(MarkPriceKlineEvent) sets it), never the general
    /// valuation price.
    void apply_funding(const MarketEvent& event) noexcept {
        const auto* funding = std::get_if<FundingEvent>(&event);
        if (!funding) return;
        // Positive funding_rate: longs pay shorts, so a long position debits
        // cash, a short one credits it.
        std::size_t i = index(funding->symbol, funding->venue);
        cash_ -= positions_[i] * funding_mark_price_[i] * funding->funding_rate;
    }

    /// Marks (symbol, venue) at whichever scalar price `event` carries:
    /// Trade's price, Kline's close, or MarkPriceKline's close. Feeds
    /// equity(). MarkPriceKline also updates funding_mark_price_, kept as
    /// a separate array so a later Trade/Kline can update valuation
    /// without also becoming what funding settles against.
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

    /// No default for `venue`: (symbol, venue) together identify an
    /// instrument, symbol alone doesn't.
    Qty position(SymbolId symbol, VenueId venue) const noexcept {
        return positions_[index(symbol, venue)];
    }

    Notional cash() const noexcept { return cash_; }

    /// cash() plus every position's mark-to-market value. Linear scan, not
    /// tracked incrementally, called at most once per Engine::step(),
    /// cheap next to that cadence.
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
/// Portfolio.
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
inline constexpr std::array<std::size_t, 1> kTrivialCounts{1};
}  // namespace detail

static_assert(PortfolioLike<Portfolio<detail::kTrivialCounts>>);

}  // namespace qp
