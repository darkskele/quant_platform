#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "python_strategy.hpp"
#include "types.hpp"

namespace py = pybind11;
using qp::strategy::python::PythonStrategy;

PYBIND11_EMBEDDED_MODULE(qp_test_types_strategy, m) {
    py::class_<qp::Intent>(m, "Intent")
        .def(py::init([](std::uint16_t ex, std::uint16_t mk, std::uint16_t sy, qp::Qty q) {
                 return qp::Intent{
                     .exchange = ex, .market = mk, .symbol = sy, .target_position = q};
             }),
             py::arg("exchange"), py::arg("market"), py::arg("symbol"), py::arg("target_position"))
        .def_readwrite("exchange", &qp::Intent::exchange)
        .def_readwrite("market", &qp::Intent::market)
        .def_readwrite("symbol", &qp::Intent::symbol)
        .def_readwrite("target_position", &qp::Intent::target_position);
}

class PythonStrategyTest : public ::testing::Test {
   protected:
    static py::scoped_interpreter& guard() {
        static py::scoped_interpreter g;
        return g;
    }

    void SetUp() override { guard(); }
};

TEST_F(PythonStrategyTest, OnTimerWithNoneCallbackReturnsEmpty) {
    PythonStrategy<> strategy{py::none(), py::none()};
    EXPECT_TRUE(strategy.on_timer(0).empty());
}

TEST_F(PythonStrategyTest, OnEventWithNoneCallbackReturnsEmpty) {
    PythonStrategy<> strategy{py::none(), py::none()};
    qp::MarketEvent  evt;
    evt.base    = {.kind = qp::EventKind::Trade, .ts = 0};
    evt.payload = qp::TradeEvent{.price = 100.0, .qty = 1.0};
    EXPECT_TRUE(strategy.on_event(evt).empty());
}

TEST_F(PythonStrategyTest, OnTimerReturnsIntentsFromPython) {
    py::module_ types = py::module_::import("qp_test_types_strategy");
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "def cb(ts):\n"
        "    return [\n"
        "        types.Intent(exchange=0, market=2, symbol=1, target_position=3.5),\n"
        "        types.Intent(exchange=0, market=5, symbol=4, target_position=-6.0),\n"
        "    ]\n",
        py::globals(), locals);

    PythonStrategy<> strategy{py::none(), locals["cb"]};
    auto             intents = strategy.on_timer(42);

    ASSERT_EQ(intents.size(), 2u);
    EXPECT_EQ(intents[0].symbol, 1u);
    EXPECT_EQ(intents[0].market, 2u);
    EXPECT_DOUBLE_EQ(intents[0].target_position, 3.5);
    EXPECT_EQ(intents[1].symbol, 4u);
    EXPECT_EQ(intents[1].market, 5u);
    EXPECT_DOUBLE_EQ(intents[1].target_position, -6.0);
}

TEST_F(PythonStrategyTest, CallbackReturningNoneMeansEmpty) {
    py::dict locals;
    py::exec("cb = lambda t: None\n", py::globals(), locals);

    PythonStrategy<> strategy{py::none(), locals["cb"]};
    EXPECT_TRUE(strategy.on_timer(0).empty());
}

TEST_F(PythonStrategyTest, BufferIsReusedAcrossCalls) {
    py::module_ types = py::module_::import("qp_test_types_strategy");
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "state = [0]\n"
        "def cb(ts):\n"
        "    state[0] += 1\n"
        "    return [types.Intent(exchange=0, market=0, symbol=state[0], "
        "target_position=float(state[0]))]\n",
        py::globals(), locals);

    PythonStrategy<> strategy{py::none(), locals["cb"]};

    auto first = strategy.on_timer(0);
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first[0].symbol, 1u);

    auto second = strategy.on_timer(1);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(second[0].symbol, 2u);
}
