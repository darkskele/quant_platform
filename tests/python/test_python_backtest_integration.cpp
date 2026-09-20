#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <optional>
#include <string>

#include "exchange.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "python/python_binance_historical_backtest.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace py      = pybind11;
namespace binance = qp::data_source::source::venue::binance;

using qp::Subscription;
using qp::SubscriptionBuilder;
using Matcher  = qp::execution::sim::matcher::last_trade::LastTradeMatcher;
using Backtest = qp::backtest::python::PythonBinanceHistoricalBacktest<Matcher>;

/// The subset of the binding surface these callbacks touch. The shipped module
/// cannot be imported into an embedded interpreter, so the types it needs are
/// registered here instead.
PYBIND11_EMBEDDED_MODULE(qp_test_binance_types, m) {
    py::enum_<qp::EventKind>(m, "EventKind")
        .value("Funding", qp::EventKind::Funding)
        .value("Kline", qp::EventKind::Kline)
        .value("MarkPriceKline", qp::EventKind::MarkPriceKline);

    py::enum_<qp::Side>(m, "Side").value("Buy", qp::Side::Buy).value("Sell", qp::Side::Sell);

    py::enum_<qp::risk::RiskOutcome>(m, "RiskOutcome")
        .value("Approved", qp::risk::RiskOutcome::Approved)
        .value("Rejected", qp::risk::RiskOutcome::Rejected);

    py::class_<qp::EventBase>(m, "EventBase")
        .def_readonly("kind", &qp::EventBase::kind)
        .def_readonly("exchange", &qp::EventBase::exchange)
        .def_readonly("market", &qp::EventBase::market)
        .def_readonly("symbol", &qp::EventBase::symbol)
        .def_readonly("ts", &qp::EventBase::ts);

    py::class_<qp::MarketEvent>(m, "MarketEvent").def_readonly("base", &qp::MarketEvent::base);

    py::class_<qp::Intent>(m, "Intent")
        .def(py::init([](std::uint16_t ex, std::uint16_t mk, std::uint16_t sy, qp::Qty q) {
            return qp::Intent{.exchange = ex, .market = mk, .symbol = sy, .target_position = q};
        }))
        .def_readonly("exchange", &qp::Intent::exchange)
        .def_readonly("market", &qp::Intent::market)
        .def_readonly("symbol", &qp::Intent::symbol)
        .def_readonly("target_position", &qp::Intent::target_position);

    py::class_<qp::Order>(m, "Order")
        .def(py::init([](qp::OrderId id, std::uint16_t ex, std::uint16_t mk, std::uint16_t sy,
                         qp::Side side, qp::Qty q) {
            return qp::Order{
                .id = id, .exchange = ex, .market = mk, .symbol = sy, .side = side, .qty = q};
        }));

    py::class_<qp::risk::RiskDecision>(m, "RiskDecision")
        .def(py::init([](qp::risk::RiskOutcome outcome, std::optional<qp::Order> order) {
            return qp::risk::RiskDecision{.outcome = outcome, .order = order};
        }));
}

namespace {

// 2025-01-01 and 2025-02-01 as nanoseconds. One monthly file per stream, which
// is as small as a real fetch gets while still crossing a file boundary.
constexpr qp::Timestamp kFrom = 1735689600000000000LL;
constexpr qp::Timestamp kTo   = 1738368000000000000LL;

constexpr std::uint16_t kUsdM = static_cast<std::uint16_t>(binance::BinanceMarket::UsdM);

Subscription one_symbol() {
    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, kUsdM, "BTCUSDT");
    return std::move(builder).build();
}

/// Hourly bars plus funding, so two streams interleave on the merge rather
/// than one running out before the other starts.
Backtest::Config config() {
    binance::HttpFetchPoolConfig pool;
    pool.workers     = 4;
    pool.max_retries = 2;
    return Backtest::Config{
        .streams = {binance::StreamSpec{binance::EndpointKind::Klines, "1h"},
                    binance::StreamSpec{binance::EndpointKind::FundingRate, {}}},
        .pool    = pool,
        .cadence = binance::Cadence::Monthly,
        .from    = kFrom,
        .to      = kTo,
    };
}

/// Registers the callback argument types. Without it every strategy call fails
/// converting its argument, which reads as a strategy error.
void register_types() { py::module_::import("qp_test_binance_types"); }

/// Runs with the GIL released, the way the binding does, and turns a deadlock
/// into a failure instead of a hung suite. The window is wide because a
/// sanitized build fetches and parses real files.
template <class Fn>
void run_guarded(Fn&& fn, std::chrono::seconds timeout = std::chrono::seconds{600}) {
    std::future<void> done;
    {
        py::gil_scoped_release release;
        done = std::async(std::launch::async, [&] { fn(); });

        if (done.wait_for(timeout) != std::future_status::ready) {
            ADD_FAILURE() << "run() never returned, the backtest deadlocked";
            // The future's destructor would block on the hung thread, so the
            // whole binary would hang here instead of reporting.
            std::abort();
        }
    }
    done.get();
}

std::size_t rows_parsed(const Backtest& backtest) {
    std::size_t total = 0;
    for (const auto& report : backtest.reports()) total += report.stats.rows_parsed;
    return total;
}

}  // namespace

// Fetch workers, the source thread and the engine thread all run while python
// holds and releases the GIL per event. That is the whole point of running this
// one under the sanitizers.
TEST(PythonBacktestIntegration, EveryFetchedRowReachesThePythonStrategy) {
    py::scoped_interpreter guard;
    register_types();

    Backtest backtest(one_symbol(), config(),
                      [](qp::Portfolio& book, const Subscription&) { return Matcher{book}; });
    backtest.plan();

    auto state          = py::module_::import("types").attr("SimpleNamespace")();
    state.attr("n")     = 0;
    state.attr("last")  = 0;
    state.attr("order") = true;
    py::exec(R"(
def make(state):
    def on_event(event):
        if event.base.ts < state.last:
            state.order = False
        state.last = event.base.ts
        state.n += 1
        return []
    return on_event
)");
    backtest.set_on_event(py::globals()["make"](state));

    run_guarded([&] { backtest.run(); });

    const auto parsed = rows_parsed(backtest);
    EXPECT_GT(parsed, 0u);
    EXPECT_EQ(state.attr("n").cast<std::size_t>(), parsed);
    EXPECT_TRUE(state.attr("order").cast<bool>()) << "events reached python out of order";

    const auto stats = backtest.fetch_stats();
    EXPECT_EQ(stats.completed_failed, 0u);
    EXPECT_GT(stats.bytes_inflated, stats.bytes_fetched);
}

// The intent crosses into python, comes back as an order, and settles in the
// book. Exercises the risk callback on the same thread the strategy runs on.
TEST(PythonBacktestIntegration, IntentsFromPythonFillAndMoveTheBook) {
    py::scoped_interpreter guard;
    register_types();

    Backtest backtest(one_symbol(), config(),
                      [](qp::Portfolio& book, const Subscription&) { return Matcher{book}; });
    backtest.plan();

    auto state           = py::module_::import("types").attr("SimpleNamespace")();
    state.attr("orders") = 0;
    py::exec(R"(
import qp_test_binance_types as t

def make(state):
    def on_event(event):
        if event.base.kind == t.EventKind.Funding:
            return [t.Intent(event.base.exchange, event.base.market, event.base.symbol, 1.0)]
        return []

    def check(intent):
        state.orders += 1
        return t.RiskDecision(
            t.RiskOutcome.Approved,
            t.Order(state.orders, intent.exchange, intent.market, intent.symbol,
                    t.Side.Buy, intent.target_position),
        )

    return on_event, check
)");
    auto made = py::globals()["make"](state);
    backtest.set_on_event(made[py::int_(0)]);
    backtest.set_check(made[py::int_(1)]);

    run_guarded([&] { backtest.run(); });

    EXPECT_GT(state.attr("orders").cast<int>(), 0) << "risk gate never saw an intent";

    const auto results = backtest.results();
    EXPECT_NE(results.final_cash, results.final_equity);
    bool charged = false;
    for (auto fee : results.fees) charged = charged || fee > 0.0;
    EXPECT_TRUE(charged) << "a filled order paid no fee";
}
