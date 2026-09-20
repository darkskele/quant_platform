#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <string>
#include <tuple>

#include "exchange.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "python/python_backtest.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace py = pybind11;

using qp::MarketEvent;
using qp::Subscription;
using qp::SubscriptionBuilder;
using qp::backtest::python::PythonBacktestBase;
using Matcher = qp::execution::sim::matcher::last_trade::LastTradeMatcher;

PYBIND11_EMBEDDED_MODULE(qp_test_backtest_types, m) {
    py::class_<qp::EventBase>(m, "EventBase").def_readonly("ts", &qp::EventBase::ts);
    py::class_<qp::MarketEvent>(m, "MarketEvent").def_readonly("base", &qp::MarketEvent::base);
}

namespace {

/// Hands out a fixed number of klines, then Eof. No network, no threads.
class StubSource {
   public:
    explicit StubSource(std::size_t count) : remaining_(count) {}

    qp::data_source::source::PullResult next() {
        if (remaining_ == 0) return std::unexpected(qp::data_source::source::SourceStatus::Eof);

        MarketEvent event;
        event.base    = {.kind = qp::EventKind::Kline, .ts = static_cast<qp::Timestamp>(++ts_)};
        event.payload = qp::KlineEvent{.close = 1.0};
        --remaining_;
        return event;
    }

   private:
    std::size_t   remaining_;
    std::uint64_t ts_{0};
};

class StubBacktest : public PythonBacktestBase<StubBacktest, Matcher> {
    using Base = PythonBacktestBase<StubBacktest, Matcher>;

   public:
    StubBacktest(Subscription subscription, std::size_t events)
        : Base(std::move(subscription),
               [](qp::Portfolio& book, const Subscription&) { return Matcher{book}; }),
          source_(events) {}

    auto sources() { return std::tie(source_); }

   private:
    StubSource source_;
};

/// Registers MarketEvent with the interpreter. Without it the strategy call
/// fails converting its argument, which looks like a strategy error.
void register_event_types() { py::module_::import("qp_test_backtest_types"); }

Subscription one_symbol() {
    SubscriptionBuilder builder;
    builder.add(qp::ExchangeId::Binance, 0, "BTCUSDT");
    return std::move(builder).build();
}

/// Runs with the GIL released, the way the binding does, and turns a deadlock
/// into a failure instead of a hung suite.
template <class Fn>
void run_guarded(Fn&& fn) {
    std::future<void> done;
    {
        // Released here, on the thread that actually holds it. The engine
        // thread cannot call back into python until this happens.
        py::gil_scoped_release release;
        done = std::async(std::launch::async, [&] { fn(); });

        if (done.wait_for(std::chrono::seconds{30}) != std::future_status::ready) {
            ADD_FAILURE() << "run() never returned, the backtest deadlocked";
            // The future's destructor would block on the hung thread, so the
            // whole binary would hang here instead of reporting.
            std::abort();
        }
    }
    done.get();
}

}  // namespace

// The engine thread calls into python for every event. Nothing may hold the
// GIL across run(), and the callback has to actually see the events.
TEST(PythonBacktest, StrategyCallbackSeesEveryEvent) {
    py::scoped_interpreter guard;
    register_event_types();

    StubBacktest backtest(one_symbol(), 512);

    auto counter      = py::module_::import("types").attr("SimpleNamespace")();
    counter.attr("n") = 0;
    py::exec(R"(
def make(counter):
    def on_event(event):
        counter.n += 1
        return []
    return on_event
)");
    auto on_event = py::globals()["make"](counter);
    backtest.set_on_event(on_event);

    run_guarded([&] { backtest.run(); });

    EXPECT_EQ(counter.attr("n").cast<int>(), 512);
}

// A raising callback kills the engine thread. Without a stop the source fills a
// sink nobody drains and the join never returns, so this hung instead of
// reporting the error.
TEST(PythonBacktest, RaisingCallbackPropagatesInsteadOfHanging) {
    py::scoped_interpreter guard;
    register_event_types();

    // More events than the sink holds, so the source blocks once the engine is
    // gone. That is the condition that used to hang.
    StubBacktest backtest(one_symbol(), 4096);

    py::exec(R"(
def boom(event):
    raise ValueError("strategy exploded")
)");
    backtest.set_on_event(py::globals()["boom"]);

    std::string message;
    run_guarded([&] {
        try {
            backtest.run();
        } catch (const std::exception& e) {
            message = e.what();
        }
    });

    // The strategy's own error, not a conversion failure standing in for it.
    EXPECT_NE(message.find("strategy exploded"), std::string::npos) << message;
}
