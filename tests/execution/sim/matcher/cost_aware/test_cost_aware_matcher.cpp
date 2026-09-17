#include <gtest/gtest.h>

#include <variant>

#include "cost_aware/cost_aware_matcher.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "exchange.hpp"
#include "matcher.hpp"
#include "portfolio.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::Order;
using qp::RejectReason;
using qp::Side;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::Timestamp;
using qp::execution::sim::matcher::Matcher;
using qp::execution::sim::matcher::cost_aware::CostAwareMatcher;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

constexpr Timestamp  kWeekNs   = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;
constexpr SlotOffset kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kMarket   = 0;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kMarket, "A");
    sub.add(ExchangeId::Binance, kMarket, "B");
    sub.add(ExchangeId::Binance, kMarket, "C");
    sub.add(ExchangeId::Binance, kMarket, "D");
    return std::move(sub).build();
}

using Book      = qp::Portfolio;
using CostMatch = CostAwareMatcher<HalfSpreadLinearImpact>;

CostMatch make_matcher(Book& book) {
    return CostMatch{book, HalfSpreadLinearImpact{book,
                                                  {CostRow{
                                                      .exchange            = kExchange,
                                                      .market              = kMarket,
                                                      .symbol              = 0,
                                                      .week_start_ns       = 0,
                                                      .half_spread_bps     = 2.0,
                                                      .impact_bps_per_unit = 0.0,
                                                      .taker_fee_bps       = 4.0,
                                                  }}}};
}

Order make_order(qp::OrderId id, SlotOffset symbol, Side side, qp::Qty qty = 1.0) {
    return Order{.id       = id,
                 .exchange = kExchange,
                 .market   = kMarket,
                 .symbol   = symbol,
                 .side     = side,
                 .qty      = qty};
}

}  // namespace

static_assert(Matcher<CostMatch>);

TEST(CostAwareMatcher, RejectsWhenNoPriceSeenYet) {
    Book book{make_subscription()};
    auto m       = make_matcher(book);
    auto outcome = m.try_fill(make_order(1, 0, Side::Buy), 10);
    ASSERT_TRUE(std::holds_alternative<qp::Reject>(outcome));
    auto& r = std::get<qp::Reject>(outcome);
    EXPECT_EQ(r.order_id, 1u);
    EXPECT_EQ(r.reason, RejectReason::NoPriceAvailable);
    EXPECT_EQ(r.ts, 10);
}

TEST(CostAwareMatcher, RejectsWhenNoCostAvailableForTsOrSymbol) {
    Book book{make_subscription()};
    auto m = make_matcher(book);
    m.on_market_event(qp::test::make_trade(1, 5, 100.0));
    auto outcome = m.try_fill(make_order(2, 1, Side::Buy), kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Reject>(outcome));
    EXPECT_EQ(std::get<qp::Reject>(outcome).reason, RejectReason::NoCostAvailable);
}

TEST(CostAwareMatcher, BuyFillPriceCrossesTheHalfSpread) {
    Book book{make_subscription()};
    auto m = make_matcher(book);
    m.on_market_event(qp::test::make_trade(0, 5, 100.0));
    auto outcome = m.try_fill(make_order(3, 0, Side::Buy), kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(outcome));
    auto& f = std::get<qp::Fill>(outcome);
    EXPECT_DOUBLE_EQ(f.price, 100.0 * (1.0 + 2.0 / 1e4));
    EXPECT_DOUBLE_EQ(f.fee, f.price * 1.0 * 4.0 / 1e4);
    EXPECT_EQ(f.side, Side::Buy);
    EXPECT_EQ(f.order_id, 3u);
}

TEST(CostAwareMatcher, SellFillPriceRecedesByHalfSpread) {
    Book book{make_subscription()};
    auto m = make_matcher(book);
    m.on_market_event(qp::test::make_trade(0, 5, 100.0));
    auto outcome = m.try_fill(make_order(4, 0, Side::Sell), kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(outcome));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(outcome).price, 100.0 * (1.0 - 2.0 / 1e4));
}

TEST(CostAwareMatcher, SequentialFillsUsesLatestReferencePrice) {
    Book book{make_subscription()};
    auto m = make_matcher(book);
    m.on_market_event(qp::test::make_trade(0, 5, 100.0));
    auto out1 = m.try_fill(make_order(1, 0, Side::Buy), kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(out1));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(out1).price, 100.0 * (1.0 + 2.0 / 1e4));

    m.on_market_event(qp::test::make_trade(0, 6, 150.0));
    auto out2 = m.try_fill(make_order(2, 0, Side::Buy), kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(out2));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(out2).price, 150.0 * (1.0 + 2.0 / 1e4));
}

TEST(CostAwareMatcher, KlineCloseUpdatesReferencePrice) {
    Book book{make_subscription()};
    auto m = make_matcher(book);
    m.on_market_event(qp::test::make_kline(0, 1, 2, 99.0, 101.0, 98.0, 100.5));
    auto outcome = m.try_fill(make_order(5, 0, Side::Buy), kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(outcome));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(outcome).price, 100.5 * (1.0 + 2.0 / 1e4));
}
