#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "cost_model/cost_model.hpp"
#include "cost_row.hpp"
#include "portfolio.hpp"
#include "types.hpp"

namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear {

/// Fill price crosses a half spread plus a per unit linear impact term.
/// Fee is a bps rate on filled notional.
template <qp::PortfolioLike Book>
class HalfSpreadLinearImpact {
   public:
    /// Groups the flat table by Book::index, packs into one contiguous
    /// vector, sorts each per-instrument block by week_start_ns, sets a
    /// span into that block for each slot.
    explicit HalfSpreadLinearImpact(std::vector<CostRow> table) {
        std::array<std::uint32_t, Book::kMaxInstruments> counts{};
        for (const auto& r : table) ++counts[Book::index(r.symbol, r.market)];

        std::array<std::uint32_t, Book::kMaxInstruments> offsets{};
        std::uint32_t                                    running = 0;
        for (std::size_t i = 0; i < Book::kMaxInstruments; ++i) {
            offsets[i] = running;
            running += counts[i];
        }

        rows_.resize(table.size());
        std::array<std::uint32_t, Book::kMaxInstruments> writes{};
        for (const auto& r : table) {
            auto slot                           = Book::index(r.symbol, r.market);
            rows_[offsets[slot] + writes[slot]] = r;
            ++writes[slot];
        }

        for (std::size_t i = 0; i < Book::kMaxInstruments; ++i) {
            auto* begin = rows_.data() + offsets[i];
            auto* end   = begin + counts[i];
            std::sort(begin, end, [](const CostRow& a, const CostRow& b) noexcept {
                return a.week_start_ns < b.week_start_ns;
            });
            by_instrument_[i] = std::span<const CostRow>{begin, counts[i]};
        }
    }

    // Spans in by_instrument_ point into rows_. Move steals the vector's
    // buffer so those pointers remain valid; copy would give the destination
    // a fresh buffer while its spans still point at the source's buffer.
    HalfSpreadLinearImpact(const HalfSpreadLinearImpact&)                = delete;
    HalfSpreadLinearImpact& operator=(const HalfSpreadLinearImpact&)     = delete;
    HalfSpreadLinearImpact(HalfSpreadLinearImpact&&) noexcept            = default;
    HalfSpreadLinearImpact& operator=(HalfSpreadLinearImpact&&) noexcept = default;

    std::optional<FillPricing> price(const Order& o, Price ref, Timestamp ts) const noexcept {
        auto series = by_instrument_[Book::index(o.symbol, o.market)];
        if (series.empty()) return std::nullopt;
        auto row = std::upper_bound(
            series.begin(), series.end(), ts,
            [](Timestamp t, const CostRow& r) noexcept { return t < r.week_start_ns; });
        if (row == series.begin()) return std::nullopt;
        --row;

        double bps  = row->half_spread_bps + row->impact_bps_per_unit * o.qty;
        double sign = (o.side == Side::Buy) ? +1.0 : -1.0;
        Price  fp   = ref * (1.0 + sign * bps / 1e4);
        return FillPricing{
            .fill_price = fp,
            .fee        = fp * o.qty * row->taker_fee_bps / 1e4,
        };
    }

    std::size_t size() const noexcept { return rows_.size(); }

   private:
    std::vector<CostRow>                                        rows_;
    std::array<std::span<const CostRow>, Book::kMaxInstruments> by_instrument_{};
};

}  // namespace qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear
