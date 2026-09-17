#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <optional>

#include "python_risk_gate.hpp"
#include "types.hpp"

namespace py = pybind11;
using qp::RiskDecision;
using qp::RiskOutcome;
using qp::risk::python::PythonRiskGate;

PYBIND11_EMBEDDED_MODULE(qp_test_types_risk, m) {
    py::class_<qp::Intent>(m, "Intent")
        .def(py::init<>())
        .def_readwrite("exchange", &qp::Intent::exchange)
        .def_readwrite("market", &qp::Intent::market)
        .def_readwrite("symbol", &qp::Intent::symbol)
        .def_readwrite("target_position", &qp::Intent::target_position);

    py::enum_<qp::Side>(m, "Side").value("Buy", qp::Side::Buy).value("Sell", qp::Side::Sell);

    py::class_<qp::Order>(m, "Order")
        .def(py::init([](qp::OrderId id, qp::SlotOffset ex, qp::SlotOffset mk, qp::SlotOffset sy,
                         qp::Side side, qp::Qty q) {
                 return qp::Order{
                     .id = id, .exchange = ex, .market = mk, .symbol = sy, .side = side, .qty = q};
             }),
             py::arg("id"), py::arg("exchange"), py::arg("market"), py::arg("symbol"),
             py::arg("side"), py::arg("qty"))
        .def_readwrite("id", &qp::Order::id)
        .def_readwrite("exchange", &qp::Order::exchange)
        .def_readwrite("market", &qp::Order::market)
        .def_readwrite("symbol", &qp::Order::symbol)
        .def_readwrite("side", &qp::Order::side)
        .def_readwrite("qty", &qp::Order::qty);

    py::enum_<qp::RiskOutcome>(m, "RiskOutcome")
        .value("Approved", qp::RiskOutcome::Approved)
        .value("Resized", qp::RiskOutcome::Resized)
        .value("Rejected", qp::RiskOutcome::Rejected);

    py::class_<qp::RiskDecision>(m, "RiskDecision")
        .def(py::init([](qp::RiskOutcome oc, std::optional<qp::Order> ord) {
                 return qp::RiskDecision{.outcome = oc, .order = ord};
             }),
             py::arg("outcome"), py::arg("order") = std::nullopt);
}

class PythonRiskGateTest : public ::testing::Test {
   protected:
    static py::scoped_interpreter& guard() {
        static py::scoped_interpreter g;
        return g;
    }

    void SetUp() override { guard(); }
};

TEST_F(PythonRiskGateTest, CheckWithNoneCallbackRejects) {
    PythonRiskGate<> gate{py::none(), py::none()};
    auto             decision = gate.check(qp::Intent{});
    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}

TEST_F(PythonRiskGateTest, CheckReturnsPythonDecision) {
    py::module_ types = py::module_::import("qp_test_types_risk");
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "def cb(intent):\n"
        "    order = types.Order(id=7, exchange=intent.exchange, market=intent.market,\n"
        "                        symbol=intent.symbol, side=types.Side.Buy,\n"
        "                        qty=abs(intent.target_position))\n"
        "    return types.RiskDecision(outcome=types.RiskOutcome.Approved, order=order)\n",
        py::globals(), locals);

    PythonRiskGate<> gate{locals["cb"], py::none()};
    auto             decision =
        gate.check(qp::Intent{.exchange = 0, .market = 2, .symbol = 1, .target_position = 3.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Approved);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->id, 7u);
    EXPECT_EQ(decision.order->symbol, 1u);
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_EQ(decision.order->market, 2u);
    EXPECT_DOUBLE_EQ(decision.order->qty, 3.0);
}

TEST_F(PythonRiskGateTest, CheckReturningNoneIsRejected) {
    py::dict locals;
    py::exec("cb = lambda intent: None\n", py::globals(), locals);

    PythonRiskGate<> gate{locals["cb"], py::none()};
    auto             decision = gate.check(qp::Intent{});
    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}

TEST_F(PythonRiskGateTest, OnTickReturnsOrders) {
    py::module_ types = py::module_::import("qp_test_types_risk");
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "def cb():\n"
        "    return [\n"
        "        types.Order(id=1, exchange=0, market=0, symbol=10, side=types.Side.Sell, "
        "qty=1.5),\n"
        "        types.Order(id=2, exchange=0, market=1, symbol=11, side=types.Side.Buy, "
        "qty=2.5),\n"
        "    ]\n",
        py::globals(), locals);

    PythonRiskGate<> gate{py::none(), locals["cb"]};
    auto             orders = gate.on_tick();

    ASSERT_EQ(orders.size(), 2u);
    EXPECT_EQ(orders[0].id, 1u);
    EXPECT_DOUBLE_EQ(orders[0].qty, 1.5);
    EXPECT_EQ(orders[1].id, 2u);
    EXPECT_DOUBLE_EQ(orders[1].qty, 2.5);
}

TEST_F(PythonRiskGateTest, OnTickWithNoneCallbackReturnsEmpty) {
    PythonRiskGate<> gate{py::none(), py::none()};
    EXPECT_TRUE(gate.on_tick().empty());
}

TEST_F(PythonRiskGateTest, OnTickCallbackReturningNoneMeansEmpty) {
    py::dict locals;
    py::exec("cb = lambda: None\n", py::globals(), locals);

    PythonRiskGate<> gate{py::none(), locals["cb"]};
    EXPECT_TRUE(gate.on_tick().empty());
}
