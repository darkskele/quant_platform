#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include "execution_gateway.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "matcher/matcher.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::Order;
using qp::Reject;
using qp::RejectReason;
using qp::Side;
namespace exec = qp::execution;
namespace sim  = qp::execution::sim;

namespace {

constexpr std::array<std::size_t, 2> kCounts{9, 9};
using Book    = qp::Portfolio<kCounts>;
using Matcher = sim::matcher::last_trade::LastTradeMatcher<Book>;

}  // namespace

static_assert(sim::matcher::Matcher<Matcher>);
static_assert(exec::ExecutionGateway<sim::SimExecution<Matcher, Book>>);

namespace {

sim::SimExecution<Matcher, Book> make_gateway() { return {}; }

}  // namespace

TEST(SimExecution, RejectsWhenNoPriceSeenYet) {
    auto gateway = make_gateway();

    gateway.submit(Order{.id = 1, .symbol = 7, .side = Side::Buy, .qty = 1.0}, /*ts=*/100);

    ASSERT_TRUE(gateway.fills().empty());
    ASSERT_EQ(gateway.rejects().size(), 1u);
    auto& reject = gateway.rejects()[0];
    EXPECT_EQ(reject.order_id, 1u);
    EXPECT_EQ(reject.symbol, 7u);
    EXPECT_EQ(reject.ts, 100);
    EXPECT_EQ(reject.reason, RejectReason::NoPriceAvailable);
}

TEST(SimExecution, FillsAtLastTradePriceOnceOneIsSeen) {
    auto gateway = make_gateway();

    gateway.on_market_event(qp::test::make_trade(/*symbol=*/7, /*ts=*/50, /*price=*/100.0));
    gateway.submit(Order{.id = 2, .symbol = 7, .side = Side::Buy, .qty = 2.0}, /*ts=*/60);

    ASSERT_TRUE(gateway.rejects().empty());
    ASSERT_EQ(gateway.fills().size(), 1u);
    auto& fill = gateway.fills()[0];
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

    ASSERT_EQ(gateway.fills().size(), 1u);
    EXPECT_DOUBLE_EQ(gateway.fills()[0].price, 105.0);
}

TEST(SimExecution, FillsAndRejectsAreEmptyWhenNothingSubmitted) {
    auto gateway = make_gateway();
    EXPECT_TRUE(gateway.fills().empty());
    EXPECT_TRUE(gateway.rejects().empty());
}

TEST(SimExecution, FillsAndRejectsAccumulateIndependentlyWithinOneMarketEvent) {
    auto gateway = make_gateway();

    // Two orders, no trade seen yet: both reject. A trade arrives, then two
    // more orders fill. Each stream preserves its own submission order.
    gateway.submit(Order{.id = 1, .symbol = 7, .side = Side::Buy, .qty = 1.0}, 10);
    gateway.submit(Order{.id = 2, .symbol = 7, .side = Side::Buy, .qty = 1.0}, 11);
    gateway.on_market_event(qp::test::make_trade(7, 20, 50.0));
    gateway.submit(Order{.id = 3, .symbol = 7, .side = Side::Buy, .qty = 1.0}, 30);
    gateway.submit(Order{.id = 4, .symbol = 7, .side = Side::Buy, .qty = 1.0}, 31);

    // on_market_event() reset the pools, so only orders 3/4 remain — the
    // per-step boundary Engine::step() relies on (on_market_event, then
    // this step's submits, then drain).
    ASSERT_TRUE(gateway.rejects().empty());
    ASSERT_EQ(gateway.fills().size(), 2u);
    EXPECT_EQ(gateway.fills()[0].order_id, 3u);
    EXPECT_EQ(gateway.fills()[1].order_id, 4u);
}

TEST(SimExecution, DifferentSymbolsTrackIndependentPrices) {
    auto gateway = make_gateway();

    gateway.on_market_event(qp::test::make_trade(7, 10, 100.0));
    // Symbol 8 has no trade yet — must reject independently of symbol 7's price.
    gateway.submit(Order{.id = 1, .symbol = 8, .side = Side::Buy, .qty = 1.0}, 20);

    EXPECT_EQ(gateway.rejects().size(), 1u);
}

// Two venues intern the same underlying instrument to the same SymbolId —
// the matcher must key on (symbol, venue), or a spot and a perp trade for
// "symbol 7" would collide into one slot.
TEST(SimExecution, DifferentVenuesTrackIndependentPricesForTheSameSymbol) {
    auto gateway = make_gateway();

    gateway.on_market_event(qp::test::make_trade(7, 10, 100.0, 1.0, Side::Buy, /*venue=*/0));
    gateway.on_market_event(qp::test::make_trade(7, 10, 200.0, 1.0, Side::Buy, /*venue=*/1));

    gateway.submit(Order{.id = 1, .symbol = 7, .side = Side::Buy, .venue = 0, .qty = 1.0}, 20);
    gateway.submit(Order{.id = 2, .symbol = 7, .side = Side::Buy, .venue = 1, .qty = 1.0}, 20);

    ASSERT_EQ(gateway.fills().size(), 2u);
    EXPECT_EQ(gateway.fills()[0].venue, 0);
    EXPECT_DOUBLE_EQ(gateway.fills()[0].price, 100.0);
    EXPECT_EQ(gateway.fills()[1].venue, 1);
    EXPECT_DOUBLE_EQ(gateway.fills()[1].price, 200.0);
}
