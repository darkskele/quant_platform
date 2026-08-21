#include <gtest/gtest.h>

#include <variant>

#include "execution_gateway.hpp"
#include "last_trade_matcher.hpp"
#include "matcher.hpp"
#include "sim_execution.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::Order;
using qp::Reject;
using qp::RejectReason;
using qp::Side;
namespace exec = qp::execution;

static_assert(exec::Matcher<exec::LastTradeMatcher>);
static_assert(exec::ExecutionGateway<exec::SimExecution<exec::LastTradeMatcher>>);

namespace {

exec::SimExecution<exec::LastTradeMatcher> make_gateway() { return {}; }

}  // namespace

TEST(SimExecution, RejectsWhenNoPriceSeenYet) {
    auto gateway = make_gateway();

    gateway.submit(Order{.id = 1, .symbol = 7, .side = Side::Buy, .qty = 1.0}, /*ts=*/100);
    auto outcome = gateway.next_outcome();

    ASSERT_TRUE(outcome.has_value());
    ASSERT_TRUE(std::holds_alternative<Reject>(*outcome));
    auto reject = std::get<Reject>(*outcome);
    EXPECT_EQ(reject.order_id, 1u);
    EXPECT_EQ(reject.symbol, 7u);
    EXPECT_EQ(reject.ts, 100);
    EXPECT_EQ(reject.reason, RejectReason::NoPriceAvailable);
}

TEST(SimExecution, FillsAtLastTradePriceOnceOneIsSeen) {
    auto gateway = make_gateway();

    gateway.on_market_event(qp::test::make_trade(/*symbol=*/7, /*ts=*/50, /*price=*/100.0));
    gateway.submit(Order{.id = 2, .symbol = 7, .side = Side::Buy, .qty = 2.0}, /*ts=*/60);
    auto outcome = gateway.next_outcome();

    ASSERT_TRUE(outcome.has_value());
    ASSERT_TRUE(std::holds_alternative<qp::Fill>(*outcome));
    auto fill = std::get<qp::Fill>(*outcome);
    EXPECT_EQ(fill.order_id, 2u);
    EXPECT_EQ(fill.symbol, 7u);
    EXPECT_EQ(fill.ts, 60);
    EXPECT_EQ(fill.side, Side::Buy);
    EXPECT_DOUBLE_EQ(fill.price, 100.0);
    EXPECT_DOUBLE_EQ(fill.qty, 2.0);
    EXPECT_DOUBLE_EQ(fill.fee, 100.0 * 2.0 * 0.0004);
}

TEST(SimExecution, LaterTradeUpdatesThePriceUsedForTheNextFill) {
    auto gateway = make_gateway();

    gateway.on_market_event(qp::test::make_trade(7, 50, 100.0));
    gateway.on_market_event(qp::test::make_trade(7, 55, 105.0));
    gateway.submit(Order{.id = 3, .symbol = 7, .side = Side::Sell, .qty = 1.0}, 60);

    auto fill = std::get<qp::Fill>(*gateway.next_outcome());
    EXPECT_DOUBLE_EQ(fill.price, 105.0);
}

TEST(SimExecution, NextOutcomeIsNulloptWhenQueueIsEmpty) {
    auto gateway = make_gateway();
    EXPECT_FALSE(gateway.next_outcome().has_value());
}

TEST(SimExecution, OutcomesComeBackInSubmissionOrderNotGroupedByKind) {
    auto gateway = make_gateway();

    // First order: no price yet -> reject. Then a trade arrives. Second
    // order: fills. The poll must return them in that order (reject, then
    // fill) — not all rejects before all fills.
    gateway.submit(Order{.id = 1, .symbol = 7, .side = Side::Buy, .qty = 1.0}, 10);
    gateway.on_market_event(qp::test::make_trade(7, 20, 50.0));
    gateway.submit(Order{.id = 2, .symbol = 7, .side = Side::Buy, .qty = 1.0}, 30);

    EXPECT_TRUE(std::holds_alternative<Reject>(*gateway.next_outcome()));
    EXPECT_TRUE(std::holds_alternative<qp::Fill>(*gateway.next_outcome()));
    EXPECT_FALSE(gateway.next_outcome().has_value());
}

TEST(SimExecution, DifferentSymbolsTrackIndependentPrices) {
    auto gateway = make_gateway();

    gateway.on_market_event(qp::test::make_trade(7, 10, 100.0));
    // Symbol 8 has no trade yet — must reject independently of symbol 7's price.
    gateway.submit(Order{.id = 1, .symbol = 8, .side = Side::Buy, .qty = 1.0}, 20);

    EXPECT_TRUE(std::holds_alternative<Reject>(*gateway.next_outcome()));
}
