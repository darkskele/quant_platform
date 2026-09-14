#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <type_traits>
#include <variant>

#include "types.hpp"

namespace qp {

/// Net position per (symbol, market) plus cash balance.
/// Direct-indexed by (SymbolId, MarketId).
template <auto Counts>
class Portfolio {
   public:
    static constexpr std::size_t kNumMarkets     = Counts.size();
    static constexpr std::size_t kMaxInstruments = [] {
        std::size_t total = 0;
        for (std::size_t c : Counts) total += c;
        return total;
    }();

    void apply_fill(const Fill& fill) noexcept {
        std::size_t i     = index(fill.symbol, fill.market);
        Qty         delta = fill.side == Side::Buy ? fill.qty : -fill.qty;
        positions_[i] += delta;
        cash_ -= delta * fill.price + fill.fee;  // buying costs cash, fee always does
        fees_[i] += fill.fee;
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
        std::size_t i    = index(funding->symbol, funding->market);
        Notional    flow = positions_[i] * funding_mark_price_[i] * funding->funding_rate;
        cash_ -= flow;
        // Split settled funding into its two signs branchlessly.
        funding_paid_[i] += std::max(flow, 0.0);
        funding_received_[i] -= std::min(flow, 0.0);
    }

    /// Marks (symbol, market) at whichever scalar price `event` carries:
    /// Trade's price, Kline's close, or MarkPriceKline's close. Feeds
    /// equity(). MarkPriceKline also updates funding_mark_price_, kept as
    /// a separate array so a later Trade/Kline can update valuation
    /// without also becoming what funding settles against.
    void apply_mark_price(const MarketEvent& event) noexcept {
        std::visit(
            [this](const auto& e) {
                using E = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<E, TradeEvent>) {
                    mark(e.symbol, e.market, e.price);
                } else if constexpr (std::is_same_v<E, KlineEvent>) {
                    mark(e.symbol, e.market, e.close);
                } else if constexpr (std::is_same_v<E, MarkPriceKlineEvent>) {
                    mark(e.symbol, e.market, e.close);
                    mark_funding(e.symbol, e.market, e.close);
                }
            },
            event);
    }

    /// No default for `market`: (symbol, market) together identify an
    /// instrument, symbol alone doesn't.
    Qty position(SymbolId symbol, MarketId market) const noexcept {
        return positions_[index(symbol, market)];
    }

    Notional cash() const noexcept { return cash_; }

    /// Attribution, cumulative per instrument. fees is always positive,
    /// funding_paid/received are the two signs of settled funding split
    /// apart. mark is the latest valuation price, for a basis split.
    Notional fees(SymbolId symbol, MarketId market) const noexcept {
        return fees_[index(symbol, market)];
    }

    Notional funding_paid(SymbolId symbol, MarketId market) const noexcept {
        return funding_paid_[index(symbol, market)];
    }

    Notional funding_received(SymbolId symbol, MarketId market) const noexcept {
        return funding_received_[index(symbol, market)];
    }

    Price mark(SymbolId symbol, MarketId market) const noexcept {
        return mark_price_[index(symbol, market)];
    }

    /// cash() plus every position's mark-to-market value. Linear scan, not
    /// tracked incrementally, called at most once per Engine::step(),
    /// cheap next to that cadence.
    Notional equity() const noexcept {
        Notional total = cash_;
        for (std::size_t i = 0; i < kMaxInstruments; ++i) total += positions_[i] * mark_price_[i];
        return total;
    }

    static constexpr std::size_t index(SymbolId symbol, MarketId market) noexcept {
        assert(market < kNumMarkets);
        assert(symbol < Counts[market]);
        return kPrefix[market] + symbol;
    }

   private:
    static constexpr auto kPrefix = [] {
        std::array<std::size_t, kNumMarkets> prefix{};
        std::size_t                          running = 0;
        for (std::size_t i = 0; i < kNumMarkets; ++i) {
            prefix[i] = running;
            running += Counts[i];
        }
        return prefix;
    }();

    void mark(SymbolId symbol, MarketId market, Price price) noexcept {
        mark_price_[index(symbol, market)] = price;
    }

    void mark_funding(SymbolId symbol, MarketId market, Price price) noexcept {
        funding_mark_price_[index(symbol, market)] = price;
    }

    std::array<Qty, kMaxInstruments>      positions_{};
    std::array<Price, kMaxInstruments>    mark_price_{};
    std::array<Price, kMaxInstruments>    funding_mark_price_{};
    std::array<Notional, kMaxInstruments> fees_{};
    std::array<Notional, kMaxInstruments> funding_paid_{};
    std::array<Notional, kMaxInstruments> funding_received_{};
    Notional                              cash_{0.0};
};

/// The seam Engine/Strategy/RiskGate depend on instead of the concrete
/// Portfolio.
template <class T>
concept PortfolioLike =
    requires(T p, const Fill& fill, const MarketEvent& event, SymbolId symbol, MarketId market) {
        { T::kMaxInstruments } -> std::convertible_to<std::size_t>;
        { T::index(symbol, market) } -> std::same_as<std::size_t>;
        { p.apply_fill(fill) } -> std::same_as<void>;
        { p.apply_funding(event) } -> std::same_as<void>;
        { p.apply_mark_price(event) } -> std::same_as<void>;
        { p.position(symbol, market) } -> std::same_as<Qty>;
        { p.equity() } -> std::same_as<Notional>;
    };

namespace detail {
inline constexpr std::array<std::size_t, 1> kTrivialCounts{1};
}  // namespace detail

static_assert(PortfolioLike<Portfolio<detail::kTrivialCounts>>);

}  // namespace qp
