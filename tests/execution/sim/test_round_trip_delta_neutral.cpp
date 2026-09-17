#include <gtest/gtest.h>

#include <span>
#include <vector>

#include "cost_aware/cost_aware_matcher.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "exchange.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "subscription.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::ExchangeId;
using qp::Order;
using qp::Side;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::Timestamp;

namespace sim = qp::execution::sim;
using qp::execution::sim::matcher::cost_aware::CostAwareMatcher;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

constexpr std::uint16_t kExchange = static_cast<std::uint16_t>(ExchangeId::Binance);
constexpr std::uint16_t kSpot     = 0;
constexpr std::uint16_t kPerp     = 1;
constexpr std::uint16_t kSym      = 0;

Subscription make_subscription() {
    SubscriptionBuilder sub;
    sub.add(ExchangeId::Binance, kSpot, "BTC");
    sub.add(ExchangeId::Binance, kPerp, "BTC");
    return std::move(sub).build();
}

using Book      = qp::Portfolio;
using CostMatch = CostAwareMatcher<HalfSpreadLinearImpact>;

constexpr double kRef          = 100.0;
constexpr double kHalfSpreadBp = 2.0;
constexpr double kFeeBp        = 4.0;
constexpr double kFundingRate  = 0.001;

constexpr double kEps = 1e-9;

sim::SimExecution<CostMatch> make_gateway(Book& book) {
    std::vector<CostRow> table{
        CostRow{.exchange            = kExchange,
                .market              = kSpot,
                .symbol              = kSym,
                .week_start_ns       = 0,
                .half_spread_bps     = kHalfSpreadBp,
                .impact_bps_per_unit = 0.0,
                .taker_fee_bps       = kFeeBp},
        CostRow{.exchange            = kExchange,
                .market              = kPerp,
                .symbol              = kSym,
                .week_start_ns       = 0,
                .half_spread_bps     = kHalfSpreadBp,
                .impact_bps_per_unit = 0.0,
                .taker_fee_bps       = kFeeBp},
    };
    return sim::SimExecution<CostMatch>{
        book, CostMatch{book, HalfSpreadLinearImpact{book, std::move(table)}}};
}

void submit_and_drain(sim::SimExecution<CostMatch>& gateway, Book& book,
                      std::span<const Order> orders, Timestamp ts) {
    for (const auto& o : orders) gateway.submit(o, ts);
    for (const auto& f : gateway.fills()) book.apply_fill(f);
}

Order buy(qp::OrderId id, std::uint16_t market) {
    return Order{.id       = id,
                 .exchange = kExchange,
                 .market   = market,
                 .symbol   = kSym,
                 .side     = Side::Buy,
                 .qty      = 1.0};
}

Order sell(qp::OrderId id, std::uint16_t market) {
    return Order{.id       = id,
                 .exchange = kExchange,
                 .market   = market,
                 .symbol   = kSym,
                 .side     = Side::Sell,
                 .qty      = 1.0};
}

}  // namespace

TEST(RoundTripDeltaNeutral, CashMatchesHandComputedFundingMinusSpreadAndFees) {
    Book book{make_subscription()};
    auto gateway = make_gateway(book);

    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));

    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));

    gateway.on_market_event(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));
    book.apply_mark_price(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));

    Order open_orders[] = {buy(1, kSpot), sell(2, kPerp)};
    submit_and_drain(gateway, book, open_orders, 10);

    EXPECT_NEAR(book.cash(), -0.12, kEps);
    EXPECT_DOUBLE_EQ(book.position(kExchange, kSpot, kSym), +1.0);
    EXPECT_DOUBLE_EQ(book.position(kExchange, kPerp, kSym), -1.0);

    gateway.on_market_event(qp::test::make_funding(kSym, 20, kFundingRate, kPerp));
    book.apply_funding(qp::test::make_funding(kSym, 20, kFundingRate, kPerp));
    EXPECT_NEAR(book.cash(), -0.12 + 0.1, kEps);

    Order close_orders[] = {sell(3, kSpot), buy(4, kPerp)};
    submit_and_drain(gateway, book, close_orders, 30);

    EXPECT_NEAR(book.cash(), -0.14, kEps);
    EXPECT_DOUBLE_EQ(book.position(kExchange, kSpot, kSym), 0.0);
    EXPECT_DOUBLE_EQ(book.position(kExchange, kPerp, kSym), 0.0);
}

TEST(RoundTripDeltaNeutral, NegativeFundingBillsTheShort) {
    Book book{make_subscription()};
    auto gateway = make_gateway(book);

    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));
    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));
    gateway.on_market_event(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));
    book.apply_mark_price(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));

    Order open_orders[] = {buy(1, kSpot), sell(2, kPerp)};
    submit_and_drain(gateway, book, open_orders, 10);
    EXPECT_NEAR(book.cash(), -0.12, kEps);

    gateway.on_market_event(qp::test::make_funding(kSym, 20, -kFundingRate, kPerp));
    book.apply_funding(qp::test::make_funding(kSym, 20, -kFundingRate, kPerp));
    EXPECT_NEAR(book.cash(), -0.12 - 0.1, kEps);
}
