#pragma once
#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <variant>
#include <vector>

#include "subscription.hpp"
#include "types.hpp"

namespace qp {

class Portfolio {
   public:
    explicit Portfolio(const Subscription& sub)
        : max_market_(sub.max_market()),
          max_symbol_(sub.max_symbol()),
          positions_(sub.total_slots(), 0.0),
          mark_price_(sub.total_slots(), 0.0),
          funding_mark_price_(sub.total_slots(), 0.0),
          fees_(sub.total_slots(), 0.0),
          funding_paid_(sub.total_slots(), 0.0),
          funding_received_(sub.total_slots(), 0.0) {}

    std::size_t index(SlotOffset exchange, SlotOffset market, SlotOffset symbol) const noexcept {
        return std::size_t{exchange} * max_market_ * max_symbol_ +
               std::size_t{market} * max_symbol_ + std::size_t{symbol};
    }

    void apply_fill(const Fill& fill) noexcept {
        std::size_t i     = index(fill.exchange, fill.market, fill.symbol);
        Qty         delta = fill.side == Side::Buy ? fill.qty : -fill.qty;
        positions_[i] += delta;
        cash_ -= delta * fill.price + fill.fee;
        fees_[i] += fill.fee;
    }

    void apply_funding(const MarketEvent& event) noexcept {
        const auto* funding = std::get_if<FundingEvent>(&event.payload);
        if (!funding) return;
        std::size_t i    = index(event.base.exchange, event.base.market, event.base.symbol);
        Notional    flow = positions_[i] * funding_mark_price_[i] * funding->funding_rate;
        cash_ -= flow;
        funding_paid_[i] += std::max(flow, 0.0);
        funding_received_[i] -= std::min(flow, 0.0);
    }

    void apply_mark_price(const MarketEvent& event) noexcept {
        std::visit(
            [this, &event](const auto& p) {
                using P = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<P, TradeEvent>) {
                    mark(event.base, p.price);
                } else if constexpr (std::is_same_v<P, KlineEvent>) {
                    mark(event.base, p.close);
                } else if constexpr (std::is_same_v<P, MarkPriceKlineEvent>) {
                    mark(event.base, p.close);
                    mark_funding(event.base, p.close);
                }
            },
            event.payload);
    }

    Qty position(SlotOffset exchange, SlotOffset market, SlotOffset symbol) const noexcept {
        return positions_[index(exchange, market, symbol)];
    }

    Notional cash() const noexcept { return cash_; }

    Notional fees(SlotOffset exchange, SlotOffset market, SlotOffset symbol) const noexcept {
        return fees_[index(exchange, market, symbol)];
    }

    Notional funding_paid(SlotOffset exchange, SlotOffset market,
                          SlotOffset symbol) const noexcept {
        return funding_paid_[index(exchange, market, symbol)];
    }

    Notional funding_received(SlotOffset exchange, SlotOffset market,
                              SlotOffset symbol) const noexcept {
        return funding_received_[index(exchange, market, symbol)];
    }

    Price mark(SlotOffset exchange, SlotOffset market, SlotOffset symbol) const noexcept {
        return mark_price_[index(exchange, market, symbol)];
    }

    Notional equity() const noexcept {
        Notional          total = cash_;
        const std::size_t n     = positions_.size();
        for (std::size_t i = 0; i < n; ++i) total += positions_[i] * mark_price_[i];
        return total;
    }

    std::size_t max_instruments() const noexcept { return positions_.size(); }

   private:
    void mark(const EventBase& base, Price price) noexcept {
        mark_price_[index(base.exchange, base.market, base.symbol)] = price;
    }

    void mark_funding(const EventBase& base, Price price) noexcept {
        funding_mark_price_[index(base.exchange, base.market, base.symbol)] = price;
    }

    SlotOffset            max_market_;
    SlotOffset            max_symbol_;
    std::vector<Qty>      positions_;
    std::vector<Price>    mark_price_;
    std::vector<Price>    funding_mark_price_;
    std::vector<Notional> fees_;
    std::vector<Notional> funding_paid_;
    std::vector<Notional> funding_received_;
    Notional              cash_{0.0};
};

}  // namespace qp
