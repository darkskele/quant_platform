#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

#include "cost_aware/cost_model/cost_model.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "portfolio.hpp"
#include "types.hpp"

using qp::Order;
using qp::Price;
using qp::Side;
using qp::Timestamp;
using qp::execution::sim::matcher::cost_aware::cost_model::CostModel;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

constexpr std::array<std::size_t, 1> kCounts{4};
using Book = qp::Portfolio<kCounts>;
using Cost = HalfSpreadLinearImpact<Book>;

constexpr Timestamp kWeekNs = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;

CostRow row(qp::SymbolId sym, qp::VenueId ven, Timestamp week_start_ns, double half_spread,
            double impact, double fee) {
    return CostRow{
        .symbol              = sym,
        .venue               = ven,
        .week_start_ns       = week_start_ns,
        .half_spread_bps     = half_spread,
        .impact_bps_per_unit = impact,
        .taker_fee_bps       = fee,
    };
}

}  // namespace

static_assert(CostModel<Cost>);

TEST(HalfSpreadLinearImpact, BuyPaysHalfSpreadWhenImpactIsZero) {
    Cost  cost{{row(1, 0, 0, /*half=*/5.0, /*imp=*/0.0, /*fee=*/0.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    auto  out = cost.price(o, 100.0, /*ts=*/kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, SellReceivesLessByHalfSpreadWhenImpactIsZero) {
    Cost  cost{{row(1, 0, 0, 5.0, 0.0, 0.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Sell, .venue = 0, .qty = 1.0};
    auto  out = cost.price(o, 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 - 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, ImpactScalesLinearlyInQty) {
    Cost  cost{{row(1, 0, 0, 0.0, 2.0, 0.0)}};
    Order half{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 0.5};
    Order full{.id = 1, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    auto  h = cost.price(half, 100.0, kWeekNs);
    auto  f = cost.price(full, 100.0, kWeekNs);
    ASSERT_TRUE(h.has_value());
    ASSERT_TRUE(f.has_value());
    double half_lift = h->fill_price / 100.0 - 1.0;
    double full_lift = f->fill_price / 100.0 - 1.0;
    EXPECT_DOUBLE_EQ(full_lift, 2.0 * half_lift);
}

TEST(HalfSpreadLinearImpact, FeeIsBpsOfFilledNotional) {
    Cost  cost{{row(1, 0, 0, 0.0, 0.0, 4.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 2.0};
    auto  out = cost.price(o, 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    // With zero spread and impact, fill_price = ref = 100. Fee = 100 * 2 * 4/1e4.
    EXPECT_DOUBLE_EQ(out->fee, 100.0 * 2.0 * 4.0 / 1e4);
}

TEST(HalfSpreadLinearImpact, MissesWhenTsPredatesEveryRow) {
    Cost  cost{{row(1, 0, /*week_start=*/kWeekNs, 5.0, 0.0, 4.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    // ts before the earliest week: no row available yet.
    EXPECT_FALSE(cost.price(o, 100.0, /*ts=*/kWeekNs - 1).has_value());
}

TEST(HalfSpreadLinearImpact, MissesWhenSymbolNotInTable) {
    Cost  cost{{row(1, 0, 0, 5.0, 0.0, 4.0)}};
    Order o{.id = 0, .symbol = 2, .side = Side::Buy, .venue = 0, .qty = 1.0};
    EXPECT_FALSE(cost.price(o, 100.0, kWeekNs).has_value());
}

TEST(HalfSpreadLinearImpact, LatestApplicableWeekWins) {
    // Three consecutive weeks for the same instrument. A trade in the middle
    // week must use the middle week's row, not the earliest or latest.
    Cost  cost{{
        row(1, 0, /*week_start=*/0 * kWeekNs, /*half=*/1.0, 0.0, 0.0),
        row(1, 0, /*week_start=*/1 * kWeekNs, /*half=*/2.0, 0.0, 0.0),
        row(1, 0, /*week_start=*/2 * kWeekNs, /*half=*/3.0, 0.0, 0.0),
    }};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    auto  mid = cost.price(o, 100.0, /*ts=*/1 * kWeekNs + kWeekNs / 2);
    ASSERT_TRUE(mid.has_value());
    EXPECT_DOUBLE_EQ(mid->fill_price, 100.0 * (1.0 + 2.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, DoesNotSlideBetweenSymbols) {
    // Two instruments' rows share the table. Structural: sym 2's slot is
    // separate from sym 1's, so this is impossible by construction now.
    // Kept as behavioral pinning.
    Cost  cost{{
        row(1, 0, /*week_start=*/0 * kWeekNs, 5.0, 0.0, 4.0),
        row(2, 0, /*week_start=*/2 * kWeekNs, 9.0, 0.0, 4.0),
    }};
    Order o{.id = 0, .symbol = 2, .side = Side::Buy, .venue = 0, .qty = 1.0};
    // ts sits inside sym 1's range but before sym 2's earliest week.
    EXPECT_FALSE(cost.price(o, 100.0, /*ts=*/1 * kWeekNs).has_value());
}

TEST(HalfSpreadLinearImpact, EmptyTableAlwaysMisses) {
    Cost  cost{{}};
    Order o{.id = 0, .symbol = 0, .side = Side::Buy, .venue = 0, .qty = 1.0};
    EXPECT_FALSE(cost.price(o, 100.0, kWeekNs).has_value());
    EXPECT_EQ(cost.size(), 0u);
}

TEST(HalfSpreadLinearImpact, TsExactlyAtWeekStartUsesThatWeek) {
    Cost  cost{{
        row(1, 0, /*week_start=*/0, 1.0, 0.0, 0.0),
        row(1, 0, /*week_start=*/kWeekNs, 5.0, 0.0, 0.0),
    }};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    // Boundary: ts == week_start of the second row. upper_bound treats it
    // as strictly greater than the first row, so the second row applies.
    auto out = cost.price(o, 100.0, /*ts=*/kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, TsOneNsBeforeFirstRowMisses) {
    Cost  cost{{row(1, 0, /*week_start=*/kWeekNs, 5.0, 0.0, 4.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    EXPECT_FALSE(cost.price(o, 100.0, /*ts=*/kWeekNs - 1).has_value());
}

TEST(HalfSpreadLinearImpact, LatestRowPersistsIntoFarFuture) {
    Cost  cost{{row(1, 0, /*week_start=*/kWeekNs, 5.0, 0.0, 4.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    // ts arbitrarily far past the last row: latest row still wins.
    auto out = cost.price(o, 100.0, /*ts=*/1000 * kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, SpreadAndImpactCompound) {
    Cost  cost{{row(1, 0, 0, /*half=*/5.0, /*imp=*/1.0, 0.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 3.0};
    auto  out = cost.price(o, 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    // Total bps = half_spread + impact * qty = 5 + 1*3 = 8.
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 8.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, FeeAppliesToAdjustedFillPriceNotReference) {
    // Buy at ref=100 with 10bps half-spread + 4bps fee.
    // fill_price = 100.10, fee = 100.10 * 1 * 4/1e4 = 0.04004 (not 0.04).
    Cost  cost{{row(1, 0, 0, /*half=*/10.0, 0.0, /*fee=*/4.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    auto  out = cost.price(o, 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.10);
    EXPECT_DOUBLE_EQ(out->fee, 100.10 * 4.0 / 1e4);
}

TEST(HalfSpreadLinearImpact, WorksAcrossPriceMagnitudes) {
    // Spread is proportional, so PnL cost per unit scales with price.
    Cost  cost{{row(1, 0, 0, /*half=*/5.0, 0.0, 0.0)}};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    for (Price ref : {0.001, 1.0, 100.0, 50000.0, 1e9}) {
        auto out = cost.price(o, ref, kWeekNs);
        ASSERT_TRUE(out.has_value());
        EXPECT_DOUBLE_EQ(out->fill_price, ref * (1.0 + 5.0 / 1e4));
    }
}

TEST(HalfSpreadLinearImpact, MultipleInstrumentsRoutedIndependently) {
    // Symbol 1 and 3 both in the table, with different half-spreads.
    // Each order lands in its own slot; no cross-pollution.
    Cost  cost{{
        row(1, 0, 0, /*half=*/2.0, 0.0, 0.0),
        row(3, 0, 0, /*half=*/9.0, 0.0, 0.0),
    }};
    Order o1{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    Order o3{.id = 1, .symbol = 3, .side = Side::Buy, .venue = 0, .qty = 1.0};
    auto  p1 = cost.price(o1, 100.0, kWeekNs);
    auto  p3 = cost.price(o3, 100.0, kWeekNs);
    ASSERT_TRUE(p1.has_value());
    ASSERT_TRUE(p3.has_value());
    EXPECT_DOUBLE_EQ(p1->fill_price, 100.0 * (1.0 + 2.0 / 1e4));
    EXPECT_DOUBLE_EQ(p3->fill_price, 100.0 * (1.0 + 9.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, MoveConstructionPreservesLookups) {
    Cost  source{{
        row(1, 0, 0, /*half=*/5.0, 0.0, 4.0),
        row(1, 0, kWeekNs, /*half=*/7.0, 0.0, 4.0),
    }};
    Cost  moved{std::move(source)};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    auto  early = moved.price(o, 100.0, /*ts=*/kWeekNs / 2);
    auto  late  = moved.price(o, 100.0, /*ts=*/kWeekNs);
    ASSERT_TRUE(early.has_value());
    ASSERT_TRUE(late.has_value());
    EXPECT_DOUBLE_EQ(early->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
    EXPECT_DOUBLE_EQ(late->fill_price, 100.0 * (1.0 + 7.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, UnsortedInputStillSortsInternally) {
    // Reader may emit rows out of week order. Constructor must sort within
    // each instrument slot regardless of input order.
    Cost  cost{{
        row(1, 0, /*week_start=*/2 * kWeekNs, /*half=*/3.0, 0.0, 0.0),
        row(1, 0, /*week_start=*/0 * kWeekNs, /*half=*/1.0, 0.0, 0.0),
        row(1, 0, /*week_start=*/1 * kWeekNs, /*half=*/2.0, 0.0, 0.0),
    }};
    Order o{.id = 0, .symbol = 1, .side = Side::Buy, .venue = 0, .qty = 1.0};
    // Query middle week; second row's spread (2.0) should apply.
    auto mid = cost.price(o, 100.0, /*ts=*/1 * kWeekNs + kWeekNs / 2);
    ASSERT_TRUE(mid.has_value());
    EXPECT_DOUBLE_EQ(mid->fill_price, 100.0 * (1.0 + 2.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, SizeReportsTotalRowCount) {
    Cost cost{{
        row(0, 0, 0, 1.0, 0.0, 0.0),
        row(1, 0, 0, 1.0, 0.0, 0.0),
        row(1, 0, kWeekNs, 1.0, 0.0, 0.0),
        row(2, 0, 0, 1.0, 0.0, 0.0),
    }};
    EXPECT_EQ(cost.size(), 4u);
}
