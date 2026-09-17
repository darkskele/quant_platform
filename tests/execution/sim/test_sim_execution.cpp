#include <gtest/gtest.h>

#include <string>

#include "exchange.hpp"
#include "execution_gateway.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "matcher/matcher.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::Order;
using qp::Reject;
using qp::RejectReason;
using qp::Side;
using qp::SlotOffset;
using qp::Subscription;
using qp::SubscriptionBuilder;
namespace exec = qp::execution;
namespace sim  = qp::execution::sim;

namespace {

constexpr SlotOffset kExchange = static_cast<SlotOffset>(ExchangeId::Binance);
constexpr SlotOffset kUsdm     = 0;
constexpr SlotOffset kCoinm    = 1;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    for (int i = 0; i < 9; ++i) {
        sub.add(ExchangeId::Binance, kUsdm, std::string{"S"} + std::to_string(i));
        sub.add(ExchangeId::Binance, kCoinm, std::string{"S"} + std::to_string(i));
    }
    return std::move(sub).build();
}

using Book    = qp::Portfolio;
using Matcher = sim::matcher::last_trade::LastTradeMatcher;

}  // namespace

static_assert(sim::matcher::Matcher<Matcher>);
static_assert(exec::ExecutionGateway<sim::SimExecution<Matcher>>);

namespace {

struct Gateway {
    Book                       book{make_subscription()};
    sim::SimExecution<Matcher> exec;

    Gateway() : exec{book, Matcher{book}} {}
};

Order make_order(qp::OrderId id, SlotOffset symbol, Side side, qp::Qty qty,
                 SlotOffset market = kUsdm) {
    return Order{.id       = id,
                 .exchange = kExchange,
                 .market   = market,
                 .symbol   = symbol,
                 .side     = side,
                 .qty      = qty};
}

}  // namespace

TEST(SimExecution, RejectsWhenNoPriceSeenYet) {
    Gateway harness;
    auto&   gateway = harness.exec;

    gateway.submit(make_order(1, 7, Side::Buy, 1.0), 100);

    ASSERT_TRUE(gateway.fills().empty());
    ASSERT_EQ(gateway.rejects().size(), 1u);
    auto& reject = gateway.rejects()[0];
    EXPECT_EQ(reject.order_id, 1u);
    EXPECT_EQ(reject.symbol, 7u);
    EXPECT_EQ(reject.ts, 100);
    EXPECT_EQ(reject.reason, RejectReason::NoPriceAvailable);
}

TEST(SimExecution, FillsAtLastTradePriceOnceOneIsSeen) {
    Gateway harness;
    auto&   gateway = harness.exec;

    gateway.on_market_event(qp::test::make_trade(7, 50, 100.0));
    gateway.submit(make_order(2, 7, Side::Buy, 2.0), 60);

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
    Gateway harness;
    auto&   gateway = harness.exec;

    gateway.on_market_event(qp::test::make_trade(7, 50, 100.0));
    gateway.on_market_event(qp::test::make_trade(7, 55, 105.0));
    gateway.submit(make_order(3, 7, Side::Sell, 1.0), 60);

    ASSERT_EQ(gateway.fills().size(), 1u);
    EXPECT_DOUBLE_EQ(gateway.fills()[0].price, 105.0);
}

TEST(SimExecution, FillsAndRejectsAreEmptyWhenNothingSubmitted) {
    Gateway harness;
    auto&   gateway = harness.exec;
    EXPECT_TRUE(gateway.fills().empty());
    EXPECT_TRUE(gateway.rejects().empty());
}

TEST(SimExecution, FillsAndRejectsAccumulateIndependentlyWithinOneMarketEvent) {
    Gateway harness;
    auto&   gateway = harness.exec;

    gateway.submit(make_order(1, 7, Side::Buy, 1.0), 10);
    gateway.submit(make_order(2, 7, Side::Buy, 1.0), 11);
    gateway.on_market_event(qp::test::make_trade(7, 20, 50.0));
    gateway.submit(make_order(3, 7, Side::Buy, 1.0), 30);
    gateway.submit(make_order(4, 7, Side::Buy, 1.0), 31);

    ASSERT_TRUE(gateway.rejects().empty());
    ASSERT_EQ(gateway.fills().size(), 2u);
    EXPECT_EQ(gateway.fills()[0].order_id, 3u);
    EXPECT_EQ(gateway.fills()[1].order_id, 4u);
}

TEST(SimExecution, DifferentSymbolsTrackIndependentPrices) {
    Gateway harness;
    auto&   gateway = harness.exec;

    gateway.on_market_event(qp::test::make_trade(7, 10, 100.0));
    gateway.submit(make_order(1, 8, Side::Buy, 1.0), 20);

    EXPECT_EQ(gateway.rejects().size(), 1u);
}

TEST(SimExecution, DifferentVenuesTrackIndependentPricesForTheSameSymbol) {
    Gateway harness;
    auto&   gateway = harness.exec;

    gateway.on_market_event(qp::test::make_trade(7, 10, 100.0, 1.0, Side::Buy, kUsdm));
    gateway.on_market_event(qp::test::make_trade(7, 10, 200.0, 1.0, Side::Buy, kCoinm));

    gateway.submit(make_order(1, 7, Side::Buy, 1.0, kUsdm), 20);
    gateway.submit(make_order(2, 7, Side::Buy, 1.0, kCoinm), 20);

    ASSERT_EQ(gateway.fills().size(), 2u);
    EXPECT_EQ(gateway.fills()[0].market, kUsdm);
    EXPECT_DOUBLE_EQ(gateway.fills()[0].price, 100.0);
    EXPECT_EQ(gateway.fills()[1].market, kCoinm);
    EXPECT_DOUBLE_EQ(gateway.fills()[1].price, 200.0);
}
