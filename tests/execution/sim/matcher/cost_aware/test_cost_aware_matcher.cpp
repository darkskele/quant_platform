#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <variant>
#include <vector>

#include "cost_aware/cost_aware_matcher.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "matcher.hpp"
#include "portfolio.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::Order;
using qp::RejectReason;
using qp::Side;
using qp::Timestamp;
using qp::execution::sim::matcher::Matcher;
using qp::execution::sim::matcher::cost_aware::CostAwareMatcher;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

constexpr Timestamp kWeekNs = 7LL * 24LL * 60LL * 60LL * 1'000'000'000LL;

constexpr std::array<std::size_t, 1> kCounts{4};
using Book      = qp::Portfolio<kCounts>;
using CostMatch = CostAwareMatcher<Book, HalfSpreadLinearImpact<Book>>;

CostMatch make_matcher() {
    return CostMatch{HalfSpreadLinearImpact<Book>{{
        CostRow{
            .symbol              = 0,
            .venue               = 0,
            .week_start_ns       = 0,
            .half_spread_bps     = 2.0,
            .impact_bps_per_unit = 0.0,
            .taker_fee_bps       = 4.0,
        },
    }}};
}

}  // namespace

static_assert(Matcher<CostMatch>);

TEST(CostAwareMatcher, RejectsWhenNoPriceSeenYet) {
    auto m       = make_matcher();
    auto outcome = m.try_fill(Order{.id = 1, .symbol = 0, .side = Side::Buy, .qty = 1.0}, 10);
    ASSERT_TRUE(std::holds_alternative<qp::Reject>(outcome));
    auto& r = std::get<qp::Reject>(outcome);
    EXPECT_EQ(r.order_id, 1u);
    EXPECT_EQ(r.reason, RejectReason::NoPriceAvailable);
    EXPECT_EQ(r.ts, 10);
}

TEST(CostAwareMatcher, RejectsWhenNoCostAvailableForTsOrSymbol) {
    // Only symbol 0's row exists, ask for symbol 1. Ref price is set so the
    // reject reason is genuinely NoCostAvailable, not NoPriceAvailable.
    auto m = make_matcher();
    m.on_market_event(qp::test::make_trade(/*symbol=*/1, /*ts=*/5, /*price=*/100.0));
    auto outcome = m.try_fill(Order{.id = 2, .symbol = 1, .side = Side::Buy, .qty = 1.0}, kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Reject>(outcome));
    EXPECT_EQ(std::get<qp::Reject>(outcome).reason, RejectReason::NoCostAvailable);
}

TEST(CostAwareMatcher, BuyFillPriceCrossesTheHalfSpread) {
    auto m = make_matcher();
    m.on_market_event(qp::test::make_trade(/*symbol=*/0, /*ts=*/5, /*price=*/100.0));
    auto outcome = m.try_fill(Order{.id = 3, .symbol = 0, .side = Side::Buy, .qty = 1.0}, kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(outcome));
    auto& f = std::get<qp::Fill>(outcome);
    EXPECT_DOUBLE_EQ(f.price, 100.0 * (1.0 + 2.0 / 1e4));
    EXPECT_DOUBLE_EQ(f.fee, f.price * 1.0 * 4.0 / 1e4);
    EXPECT_EQ(f.side, Side::Buy);
    EXPECT_EQ(f.order_id, 3u);
}

TEST(CostAwareMatcher, SellFillPriceRecedesByHalfSpread) {
    auto m = make_matcher();
    m.on_market_event(qp::test::make_trade(0, 5, 100.0));
    auto outcome = m.try_fill(Order{.id = 4, .symbol = 0, .side = Side::Sell, .qty = 1.0}, kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(outcome));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(outcome).price, 100.0 * (1.0 - 2.0 / 1e4));
}

TEST(CostAwareMatcher, SequentialFillsUsesLatestReferencePrice) {
    // A stream of trades updates last_price_; each fill must use the
    // most-recent value for that (symbol, venue).
    auto m = make_matcher();
    m.on_market_event(qp::test::make_trade(0, 5, 100.0));
    auto out1 = m.try_fill(Order{.id = 1, .symbol = 0, .side = Side::Buy, .qty = 1.0}, kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(out1));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(out1).price, 100.0 * (1.0 + 2.0 / 1e4));

    m.on_market_event(qp::test::make_trade(0, 6, 150.0));
    auto out2 = m.try_fill(Order{.id = 2, .symbol = 0, .side = Side::Buy, .qty = 1.0}, kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(out2));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(out2).price, 150.0 * (1.0 + 2.0 / 1e4));
}

TEST(CostAwareMatcher, KlineCloseUpdatesReferencePrice) {
    auto m = make_matcher();
    m.on_market_event(qp::test::make_kline(/*symbol=*/0, /*open_time=*/1, /*close_time=*/2,
                                           /*open=*/99.0, /*high=*/101.0, /*low=*/98.0,
                                           /*close=*/100.5));
    auto outcome = m.try_fill(Order{.id = 5, .symbol = 0, .side = Side::Buy, .qty = 1.0}, kWeekNs);
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(outcome));
    EXPECT_DOUBLE_EQ(std::get<qp::Fill>(outcome).price, 100.5 * (1.0 + 2.0 / 1e4));
}
