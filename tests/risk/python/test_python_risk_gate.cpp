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
        .def_readwrite("symbol", &qp::Intent::symbol)
        .def_readwrite("venue", &qp::Intent::venue)
        .def_readwrite("target_position", &qp::Intent::target_position);

    py::enum_<qp::Side>(m, "Side").value("Buy", qp::Side::Buy).value("Sell", qp::Side::Sell);

    py::class_<qp::Order>(m, "Order")
        .def(py::init([](qp::OrderId id, qp::SymbolId s, qp::Side side, qp::VenueId v, qp::Qty q) {
                 return qp::Order{.id = id, .symbol = s, .side = side, .venue = v, .qty = q};
             }),
             py::arg("id"), py::arg("symbol"), py::arg("side"), py::arg("venue"), py::arg("qty"))
        .def_readwrite("id", &qp::Order::id)
        .def_readwrite("symbol", &qp::Order::symbol)
        .def_readwrite("side", &qp::Order::side)
        .def_readwrite("venue", &qp::Order::venue)
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
    // A single interpreter for the whole test binary; scoped_interpreter's
    // ctor can only run once per process.
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
        "    order = types.Order(id=7, symbol=intent.symbol, side=types.Side.Buy,\n"
        "                        venue=intent.venue, qty=abs(intent.target_position))\n"
        "    return types.RiskDecision(outcome=types.RiskOutcome.Approved, order=order)\n",
        py::globals(), locals);

    PythonRiskGate<> gate{locals["cb"], py::none()};
    auto decision = gate.check(qp::Intent{.symbol = 1, .venue = 2, .target_position = 3.0});

    EXPECT_EQ(decision.outcome, RiskOutcome::Approved);
    ASSERT_TRUE(decision.order.has_value());
    EXPECT_EQ(decision.order->id, 7u);
    EXPECT_EQ(decision.order->symbol, 1u);
    EXPECT_EQ(decision.order->side, qp::Side::Buy);
    EXPECT_EQ(decision.order->venue, 2u);
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
        "        types.Order(id=1, symbol=10, side=types.Side.Sell, venue=0, qty=1.5),\n"
        "        types.Order(id=2, symbol=11, side=types.Side.Buy, venue=1, qty=2.5),\n"
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
