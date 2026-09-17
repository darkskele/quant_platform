#include <gtest/gtest.h>

#include <vector>

#include "cost_aware/cost_model/cost_model.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "exchange.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::Order;
using qp::Price;
using qp::Side;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::Timestamp;
using qp::execution::sim::matcher::cost_aware::cost_model::CostModel;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

constexpr std::uint16_t kExchange = static_cast<std::uint16_t>(ExchangeId::Binance);
constexpr std::uint16_t kMarket   = 0;
constexpr Timestamp     kWeekNs   = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    sub.add(ExchangeId::Binance, kMarket, "B");
    sub.add(ExchangeId::Binance, kMarket, "C");
    sub.add(ExchangeId::Binance, kMarket, "D");
    return std::move(sub).build();
}

using Book = qp::Portfolio;
using Cost = HalfSpreadLinearImpact;

CostRow row(std::uint16_t sym, Timestamp week_start_ns, double half_spread, double impact,
            double fee) {
    return CostRow{
        .exchange            = kExchange,
        .market              = kMarket,
        .symbol              = sym,
        .week_start_ns       = week_start_ns,
        .half_spread_bps     = half_spread,
        .impact_bps_per_unit = impact,
        .taker_fee_bps       = fee,
    };
}

Order order(std::uint16_t sym, Side side, qp::Qty qty = 1.0) {
    return Order{
        .id = 0, .exchange = kExchange, .market = kMarket, .symbol = sym, .side = side, .qty = qty};
}

}  // namespace

static_assert(CostModel<Cost>);

TEST(HalfSpreadLinearImpact, BuyPaysHalfSpreadWhenImpactIsZero) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 5.0, 0.0, 0.0)}};
    auto out = cost.price(order(1, Side::Buy), 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, SellReceivesLessByHalfSpreadWhenImpactIsZero) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 5.0, 0.0, 0.0)}};
    auto out = cost.price(order(1, Side::Sell), 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 - 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, ImpactScalesLinearlyInQty) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 0.0, 2.0, 0.0)}};
    auto h = cost.price(order(1, Side::Buy, 0.5), 100.0, kWeekNs);
    auto f = cost.price(order(1, Side::Buy, 1.0), 100.0, kWeekNs);
    ASSERT_TRUE(h.has_value());
    ASSERT_TRUE(f.has_value());
    double half_lift = h->fill_price / 100.0 - 1.0;
    double full_lift = f->fill_price / 100.0 - 1.0;
    EXPECT_DOUBLE_EQ(full_lift, 2.0 * half_lift);
}

TEST(HalfSpreadLinearImpact, FeeIsBpsOfFilledNotional) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 0.0, 0.0, 4.0)}};
    auto out = cost.price(order(1, Side::Buy, 2.0), 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fee, 100.0 * 2.0 * 4.0 / 1e4);
}

TEST(HalfSpreadLinearImpact, MissesWhenTsPredatesEveryRow) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, kWeekNs, 5.0, 0.0, 4.0)}};
    EXPECT_FALSE(cost.price(order(1, Side::Buy), 100.0, kWeekNs - 1).has_value());
}

TEST(HalfSpreadLinearImpact, MissesWhenSymbolNotInTable) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 5.0, 0.0, 4.0)}};
    EXPECT_FALSE(cost.price(order(2, Side::Buy), 100.0, kWeekNs).has_value());
}

TEST(HalfSpreadLinearImpact, LatestApplicableWeekWins) {
    Book book{make_subscription()};
    Cost cost{book,
              {
                  row(1, 0 * kWeekNs, 1.0, 0.0, 0.0),
                  row(1, 1 * kWeekNs, 2.0, 0.0, 0.0),
                  row(1, 2 * kWeekNs, 3.0, 0.0, 0.0),
              }};
    auto mid = cost.price(order(1, Side::Buy), 100.0, 1 * kWeekNs + kWeekNs / 2);
    ASSERT_TRUE(mid.has_value());
    EXPECT_DOUBLE_EQ(mid->fill_price, 100.0 * (1.0 + 2.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, DoesNotSlideBetweenSymbols) {
    Book book{make_subscription()};
    Cost cost{book,
              {
                  row(1, 0 * kWeekNs, 5.0, 0.0, 4.0),
                  row(2, 2 * kWeekNs, 9.0, 0.0, 4.0),
              }};
    EXPECT_FALSE(cost.price(order(2, Side::Buy), 100.0, 1 * kWeekNs).has_value());
}

TEST(HalfSpreadLinearImpact, EmptyTableAlwaysMisses) {
    Book book{make_subscription()};
    Cost cost{book, {}};
    EXPECT_FALSE(cost.price(order(0, Side::Buy), 100.0, kWeekNs).has_value());
    EXPECT_EQ(cost.size(), 0u);
}

TEST(HalfSpreadLinearImpact, TsExactlyAtWeekStartUsesThatWeek) {
    Book book{make_subscription()};
    Cost cost{book,
              {
                  row(1, 0, 1.0, 0.0, 0.0),
                  row(1, kWeekNs, 5.0, 0.0, 0.0),
              }};
    auto out = cost.price(order(1, Side::Buy), 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, TsOneNsBeforeFirstRowMisses) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, kWeekNs, 5.0, 0.0, 4.0)}};
    EXPECT_FALSE(cost.price(order(1, Side::Buy), 100.0, kWeekNs - 1).has_value());
}

TEST(HalfSpreadLinearImpact, LatestRowPersistsIntoFarFuture) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, kWeekNs, 5.0, 0.0, 4.0)}};
    auto out = cost.price(order(1, Side::Buy), 100.0, 1000 * kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, SpreadAndImpactCompound) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 5.0, 1.0, 0.0)}};
    auto out = cost.price(order(1, Side::Buy, 3.0), 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.0 * (1.0 + 8.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, FeeAppliesToAdjustedFillPriceNotReference) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 10.0, 0.0, 4.0)}};
    auto out = cost.price(order(1, Side::Buy), 100.0, kWeekNs);
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(out->fill_price, 100.10);
    EXPECT_DOUBLE_EQ(out->fee, 100.10 * 4.0 / 1e4);
}

TEST(HalfSpreadLinearImpact, WorksAcrossPriceMagnitudes) {
    Book book{make_subscription()};
    Cost cost{book, {row(1, 0, 5.0, 0.0, 0.0)}};
    for (Price ref : {0.001, 1.0, 100.0, 50000.0, 1e9}) {
        auto out = cost.price(order(1, Side::Buy), ref, kWeekNs);
        ASSERT_TRUE(out.has_value());
        EXPECT_DOUBLE_EQ(out->fill_price, ref * (1.0 + 5.0 / 1e4));
    }
}

TEST(HalfSpreadLinearImpact, MultipleInstrumentsRoutedIndependently) {
    Book book{make_subscription()};
    Cost cost{book,
              {
                  row(1, 0, 2.0, 0.0, 0.0),
                  row(3, 0, 9.0, 0.0, 0.0),
              }};
    auto p1 = cost.price(order(1, Side::Buy), 100.0, kWeekNs);
    auto p3 = cost.price(order(3, Side::Buy), 100.0, kWeekNs);
    ASSERT_TRUE(p1.has_value());
    ASSERT_TRUE(p3.has_value());
    EXPECT_DOUBLE_EQ(p1->fill_price, 100.0 * (1.0 + 2.0 / 1e4));
    EXPECT_DOUBLE_EQ(p3->fill_price, 100.0 * (1.0 + 9.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, MoveConstructionPreservesLookups) {
    Book book{make_subscription()};
    Cost source{book,
                {
                    row(1, 0, 5.0, 0.0, 4.0),
                    row(1, kWeekNs, 7.0, 0.0, 4.0),
                }};
    Cost moved{std::move(source)};
    auto early = moved.price(order(1, Side::Buy), 100.0, kWeekNs / 2);
    auto late  = moved.price(order(1, Side::Buy), 100.0, kWeekNs);
    ASSERT_TRUE(early.has_value());
    ASSERT_TRUE(late.has_value());
    EXPECT_DOUBLE_EQ(early->fill_price, 100.0 * (1.0 + 5.0 / 1e4));
    EXPECT_DOUBLE_EQ(late->fill_price, 100.0 * (1.0 + 7.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, UnsortedInputStillSortsInternally) {
    Book book{make_subscription()};
    Cost cost{book,
              {
                  row(1, 2 * kWeekNs, 3.0, 0.0, 0.0),
                  row(1, 0 * kWeekNs, 1.0, 0.0, 0.0),
                  row(1, 1 * kWeekNs, 2.0, 0.0, 0.0),
              }};
    auto mid = cost.price(order(1, Side::Buy), 100.0, 1 * kWeekNs + kWeekNs / 2);
    ASSERT_TRUE(mid.has_value());
    EXPECT_DOUBLE_EQ(mid->fill_price, 100.0 * (1.0 + 2.0 / 1e4));
}

TEST(HalfSpreadLinearImpact, SizeReportsTotalRowCount) {
    Book book{make_subscription()};
    Cost cost{book,
              {
                  row(0, 0, 1.0, 0.0, 0.0),
                  row(1, 0, 1.0, 0.0, 0.0),
                  row(1, kWeekNs, 1.0, 0.0, 0.0),
                  row(2, 0, 1.0, 0.0, 0.0),
              }};
    EXPECT_EQ(cost.size(), 4u);
}
