#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

#include "subscription.hpp"
#include "types.hpp"

namespace qp {

/// Net position per (symbol, market) plus cash balance.
class Portfolio {
   public:
    explicit Portfolio(const SubscriptionConfig& sub) {
        std::size_t running = 0;
        for (std::size_t i = 0; i < kNumMarkets; ++i) {
            prefix_[i] = running;
            running += sub.counts[i];
        }
        prefix_[kNumMarkets] = running;
        positions_.assign(running, 0.0);
        mark_price_.assign(running, 0.0);
        funding_mark_price_.assign(running, 0.0);
        fees_.assign(running, 0.0);
        funding_paid_.assign(running, 0.0);
        funding_received_.assign(running, 0.0);
    }

    explicit Portfolio(std::span<const std::size_t> counts) : Portfolio{sub_from(counts)} {}

    void apply_fill(const Fill& fill) noexcept {
        std::size_t i     = index(fill.symbol, fill.market);
        Qty         delta = fill.side == Side::Buy ? fill.qty : -fill.qty;
        positions_[i] += delta;
        cash_ -= delta * fill.price + fill.fee;  // buying costs cash, fee always does
        fees_[i] += fill.fee;
    }

    /// Settles against the official mark price (funding_mark_price_, starts 
    /// at 0 until an apply_mark_price(MarkPriceKlineEvent) sets it).
    void apply_funding(const MarketEvent& event) noexcept {
        const auto* funding = std::get_if<FundingEvent>(&event);
        if (!funding) return;
        // Longs pay shorts, so a long position debits
        // cash, a short one credits it. @todo is this strat specific?
        std::size_t i    = index(funding->base.symbol, funding->base.market);
        Notional    flow = positions_[i] * funding_mark_price_[i] * funding->funding_rate;
        cash_ -= flow;
        // Split settled funding into its two signs branchlessly.
        funding_paid_[i] += std::max(flow, 0.0);
        funding_received_[i] -= std::min(flow, 0.0);
    }

    /// Marks (symbol, market) at whichever scalar price `event` carries.
    void apply_mark_price(const MarketEvent& event) noexcept {
        std::visit(
            [this](const auto& e) {
                using E = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<E, TradeEvent>) {
                    mark(e.base.symbol, e.base.market, e.price);
                } else if constexpr (std::is_same_v<E, KlineEvent>) {
                    mark(e.base.symbol, e.base.market, e.close);
                } else if constexpr (std::is_same_v<E, MarkPriceKlineEvent>) {
                    mark(e.base.symbol, e.base.market, e.close);
                    mark_funding(e.base.symbol, e.base.market, e.close);
                }
            },
            event);
    }

    /// No default for `market`: (symbol, market) together identify an
    /// instrument, symbol alone doesn't.
    Qty position(SymbolId symbol, Market market) const noexcept {
        return positions_[index(symbol, market)];
    }

    Notional cash() const noexcept { return cash_; }

    /// Attribution, cumulative per instrument. fees is always positive,
    /// funding_paid/received are the two signs of settled funding split
    /// apart. mark is the latest valuation price, for a basis split.
    Notional fees(SymbolId symbol, Market market) const noexcept {
        return fees_[index(symbol, market)];
    }

    Notional funding_paid(SymbolId symbol, Market market) const noexcept {
        return funding_paid_[index(symbol, market)];
    }

    Notional funding_received(SymbolId symbol, Market market) const noexcept {
        return funding_received_[index(symbol, market)];
    }

    Price mark(SymbolId symbol, Market market) const noexcept {
        return mark_price_[index(symbol, market)];
    }

    /// cash() plus every position's mark-to-market value.
    Notional equity() const noexcept {
        Notional          total = cash_;
        const std::size_t n     = positions_.size();
        for (std::size_t i = 0; i < n; ++i) total += positions_[i] * mark_price_[i];
        return total;
    }

    std::size_t index(SymbolId symbol, Market market) const noexcept {
        const auto m = static_cast<std::size_t>(market);
        assert(m < kNumMarkets);
        assert(symbol < prefix_[m + 1] - prefix_[m]);
        return prefix_[m] + symbol;
    }

    std::size_t max_instruments() const noexcept { return positions_.size(); }

   private:
    static SubscriptionConfig sub_from(std::span<const std::size_t> counts) noexcept {
        SubscriptionConfig sub{};
        for (std::size_t i = 0; i < std::min(counts.size(), sub.counts.size()); ++i)
            sub.counts[i] = counts[i];
        return sub;
    }

    void mark(SymbolId symbol, Market market, Price price) noexcept {
        mark_price_[index(symbol, market)] = price;
    }

    void mark_funding(SymbolId symbol, Market market, Price price) noexcept {
        funding_mark_price_[index(symbol, market)] = price;
    }

    std::array<std::size_t, kNumMarkets + 1> prefix_{};
    std::vector<Qty>                         positions_;
    std::vector<Price>                       mark_price_;
    std::vector<Price>                       funding_mark_price_;
    std::vector<Notional>                    fees_;
    std::vector<Notional>                    funding_paid_;
    std::vector<Notional>                    funding_received_;
    Notional                                 cash_{0.0};
};

}  // namespace qp
