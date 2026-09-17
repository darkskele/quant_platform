#pragma once
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "exchange.hpp"
#include "types.hpp"

namespace qp {

// Portfolio and the engine size their coarse-indexed
// arrays from this once, at construction
class Subscription {
   public:
    struct Instrument {
        SlotOffset exchange{};
        SlotOffset market{};
        SlotOffset symbol{};
    };

    struct MarketSlice {
        SlotOffset               market{};
        std::vector<std::string> symbols;
    };

    struct ExchangeSlice {
        SlotOffset               exchange{};
        std::vector<MarketSlice> markets;
    };

    explicit Subscription(std::vector<ExchangeSlice> exchanges)
        : exchanges_(std::move(exchanges)) {}

    std::optional<Instrument> resolve(ExchangeId exchange, SlotOffset market,
                                      std::string_view symbol) const noexcept {
        auto  ex_slot = static_cast<SlotOffset>(exchange);
        auto* ex      = find_exchange(ex_slot);
        if (!ex) return std::nullopt;
        auto* mkt = find_market(*ex, market);
        if (!mkt) return std::nullopt;
        auto it = std::find(mkt->symbols.begin(), mkt->symbols.end(), symbol);
        if (it == mkt->symbols.end()) return std::nullopt;
        return Instrument{
            .exchange = ex_slot,
            .market   = market,
            .symbol   = static_cast<SlotOffset>(it - mkt->symbols.begin()),
        };
    }

    SlotOffset max_exchange() const noexcept {
        SlotOffset m = 0;
        for (const auto& ex : exchanges_) m = std::max(m, static_cast<SlotOffset>(ex.exchange + 1));
        return m;
    }

    SlotOffset max_market() const noexcept {
        SlotOffset m = 0;
        for (const auto& ex : exchanges_)
            for (const auto& mkt : ex.markets)
                m = std::max(m, static_cast<SlotOffset>(mkt.market + 1));
        return m;
    }

    SlotOffset max_symbol() const noexcept {
        SlotOffset m = 0;
        for (const auto& ex : exchanges_)
            for (const auto& mkt : ex.markets)
                m = std::max(m, static_cast<SlotOffset>(mkt.symbols.size()));
        return m;
    }

    std::size_t total_slots() const noexcept {
        return std::size_t{max_exchange()} * max_market() * max_symbol();
    }

    std::span<const ExchangeSlice> exchanges() const noexcept { return exchanges_; }

   private:
    const ExchangeSlice* find_exchange(SlotOffset ex_slot) const noexcept {
        for (const auto& ex : exchanges_)
            if (ex.exchange == ex_slot) return &ex;
        return nullptr;
    }

    const MarketSlice* find_market(const ExchangeSlice& ex, SlotOffset market) const noexcept {
        for (const auto& m : ex.markets)
            if (m.market == market) return &m;
        return nullptr;
    }

    std::vector<ExchangeSlice> exchanges_;
};

// Build it once at startup.
class SubscriptionBuilder {
   public:
    void add(ExchangeId exchange, SlotOffset market, std::string_view symbol) {
        auto  ex_slot = static_cast<SlotOffset>(exchange);
        auto& ex      = ensure_exchange(ex_slot);
        auto& mkt     = ensure_market(ex, market);
        mkt.symbols.emplace_back(symbol);
    }

    Subscription build() && { return Subscription(std::move(exchanges_)); }

   private:
    Subscription::ExchangeSlice& ensure_exchange(SlotOffset ex_slot) {
        for (auto& ex : exchanges_)
            if (ex.exchange == ex_slot) return ex;
        exchanges_.push_back(Subscription::ExchangeSlice{.exchange = ex_slot, .markets = {}});
        return exchanges_.back();
    }

    Subscription::MarketSlice& ensure_market(Subscription::ExchangeSlice& ex, SlotOffset market) {
        for (auto& m : ex.markets)
            if (m.market == market) return m;
        ex.markets.push_back(Subscription::MarketSlice{.market = market, .symbols = {}});
        return ex.markets.back();
    }

    std::vector<Subscription::ExchangeSlice> exchanges_;
};

}  // namespace qp
