// Delta-neutral round trip through SimExecution + CostAwareMatcher +
// Portfolio, with every cash flow computed by hand: entry costs on both
// legs, one funding print against the short leg, close on both legs.
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "cost_aware/cost_aware_matcher.hpp"
#include "cost_aware/cost_model/half_spread_linear/cost_row.hpp"
#include "cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "portfolio.hpp"
#include "sim_execution.hpp"
#include "support/market_event_builders.hpp"
#include "types.hpp"

using qp::Order;
using qp::Side;
using qp::Timestamp;

namespace sim = qp::execution::sim;
using qp::execution::sim::matcher::cost_aware::CostAwareMatcher;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::CostRow;
using qp::execution::sim::matcher::cost_aware::cost_model::half_spread_linear::
    HalfSpreadLinearImpact;

namespace {

// Venue 0 holds one spot symbol, venue 1 holds one perp symbol. The
// (symbol, venue) pair identifies an instrument; symbol id 0 on both.
constexpr std::array<std::size_t, 2> kCounts{1, 1};
using Book      = qp::Portfolio<kCounts>;
using CostMatch = CostAwareMatcher<Book, HalfSpreadLinearImpact<Book>>;

constexpr qp::VenueId  kSpot = 0;
constexpr qp::VenueId  kPerp = 1;
constexpr qp::SymbolId kSym  = 0;

constexpr double kRef          = 100.0;
constexpr double kHalfSpreadBp = 2.0;
constexpr double kFeeBp        = 4.0;
constexpr double kFundingRate  = 0.001;

// Cash accumulates through six ops (open + funding + close). Rounding
// noise sits well below 1e-9, and 1e-9 is still four orders of magnitude
// tighter than any cost we care about.
constexpr double kEps = 1e-9;

sim::SimExecution<CostMatch, Book> make_gateway() {
    // One row per leg, both effective from ts=0 so any test ts inside
    // this run resolves. Same cost parameters both legs.
    std::vector<CostRow> table{
        CostRow{
            .symbol              = kSym,
            .venue               = kSpot,
            .week_start_ns       = 0,
            .half_spread_bps     = kHalfSpreadBp,
            .impact_bps_per_unit = 0.0,
            .taker_fee_bps       = kFeeBp,
        },
        CostRow{
            .symbol              = kSym,
            .venue               = kPerp,
            .week_start_ns       = 0,
            .half_spread_bps     = kHalfSpreadBp,
            .impact_bps_per_unit = 0.0,
            .taker_fee_bps       = kFeeBp,
        },
    };
    return sim::SimExecution<CostMatch, Book>{
        CostMatch{HalfSpreadLinearImpact<Book>{std::move(table)}}};
}

// Drives one step's submits into the book: submit each order, drain
// fills into portfolio, mirroring Engine::step().
void submit_and_drain(sim::SimExecution<CostMatch, Book>& gateway, Book& book,
                      std::span<const Order> orders, Timestamp ts) {
    for (const auto& o : orders) gateway.submit(o, ts);
    for (const auto& f : gateway.fills()) book.apply_fill(f);
}

}  // namespace

TEST(RoundTripDeltaNeutral, CashMatchesHandComputedFundingMinusSpreadAndFees) {
    auto gateway = make_gateway();
    Book book;

    // Prime prices on both legs and the funding mark on the perp.
    gateway.on_market_event(qp::test::make_kline(kSym, /*open_time=*/0, /*close_time=*/1, kRef,
                                                 kRef, kRef, kRef,
                                                 /*volume=*/0.0, kSpot));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));

    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));

    gateway.on_market_event(qp::test::make_mark_price_kline(kSym, /*open_time=*/1, kRef,
                                                            /*close_time=*/2, kRef, kRef, kRef,
                                                            kPerp));
    book.apply_mark_price(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));

    // Open: long 1 spot, short 1 perp.
    Order open_orders[] = {
        {.id = 1, .symbol = kSym, .side = Side::Buy, .venue = kSpot, .qty = 1.0},
        {.id = 2, .symbol = kSym, .side = Side::Sell, .venue = kPerp, .qty = 1.0},
    };
    submit_and_drain(gateway, book, open_orders, /*ts=*/10);

    // Hand-computed: two half-spread crossings and two fees, taken over
    // the exactly-symmetric ref = 100 on both legs.
    // Buy spot:  fill = 100 * (1 + 2e-4) = 100.02, fee = 100.02 * 4e-4 = 0.040008
    // Sell perp: fill = 100 * (1 - 2e-4) =  99.98, fee =  99.98 * 4e-4 = 0.039992
    // cash open = -100.020000 - 0.040008 + 99.980000 - 0.039992 = -0.12
    EXPECT_NEAR(book.cash(), -0.12, kEps);
    EXPECT_DOUBLE_EQ(book.position(kSym, kSpot), +1.0);
    EXPECT_DOUBLE_EQ(book.position(kSym, kPerp), -1.0);

    // Funding print on the perp. Short leg with positive rate earns.
    // credit = -(position * mark * rate) = -(-1 * 100 * 0.001) = +0.1
    gateway.on_market_event(qp::test::make_funding(kSym, /*ts=*/20, kFundingRate, kPerp));
    book.apply_funding(qp::test::make_funding(kSym, 20, kFundingRate, kPerp));
    EXPECT_NEAR(book.cash(), -0.12 + 0.1, kEps);

    // Close: sell the spot, buy back the perp.
    Order close_orders[] = {
        {.id = 3, .symbol = kSym, .side = Side::Sell, .venue = kSpot, .qty = 1.0},
        {.id = 4, .symbol = kSym, .side = Side::Buy, .venue = kPerp, .qty = 1.0},
    };
    submit_and_drain(gateway, book, close_orders, /*ts=*/30);

    // Close mirrors open: another -0.12 in spread + fees.
    // Net PnL = funding (+0.10) - round-trip cost (0.24) = -0.14.
    EXPECT_NEAR(book.cash(), -0.14, kEps);
    EXPECT_DOUBLE_EQ(book.position(kSym, kSpot), 0.0);
    EXPECT_DOUBLE_EQ(book.position(kSym, kPerp), 0.0);
}

TEST(RoundTripDeltaNeutral, NegativeFundingBillsTheShort) {
    auto gateway = make_gateway();
    Book book;

    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kSpot));
    gateway.on_market_event(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));
    book.apply_mark_price(qp::test::make_kline(kSym, 0, 1, kRef, kRef, kRef, kRef, 0.0, kPerp));
    gateway.on_market_event(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));
    book.apply_mark_price(
        qp::test::make_mark_price_kline(kSym, 1, kRef, 2, kRef, kRef, kRef, kPerp));

    Order open_orders[] = {
        {.id = 1, .symbol = kSym, .side = Side::Buy, .venue = kSpot, .qty = 1.0},
        {.id = 2, .symbol = kSym, .side = Side::Sell, .venue = kPerp, .qty = 1.0},
    };
    submit_and_drain(gateway, book, open_orders, 10);
    EXPECT_NEAR(book.cash(), -0.12, kEps);

    // Negative funding on a short: short pays. debit = 0.1.
    gateway.on_market_event(qp::test::make_funding(kSym, 20, -kFundingRate, kPerp));
    book.apply_funding(qp::test::make_funding(kSym, 20, -kFundingRate, kPerp));
    EXPECT_NEAR(book.cash(), -0.12 - 0.1, kEps);
}
