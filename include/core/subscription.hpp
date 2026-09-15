#pragma once
#include <array>
#include <cstddef>

#include "markets.hpp"

namespace qp {

/// The market universe this run subscribes to, decided at engine
/// composition time.
struct SubscriptionConfig {
    std::array<std::size_t, kNumMarkets> counts{};

    constexpr std::size_t count(Market m) const noexcept {
        return counts[static_cast<std::size_t>(m)];
    }

    constexpr void set(Market m, std::size_t n) noexcept {
        counts[static_cast<std::size_t>(m)] = n;
    }

    constexpr std::size_t total() const noexcept {
        std::size_t t = 0;
        for (auto c : counts) t += c;
        return t;
    }
};

}  // namespace qp
