#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "python_risk_gate.hpp"
#include "support/python_interpreter.hpp"
#include "types.hpp"

namespace py = pybind11;
using qp::risk::RiskDecision;
using qp::risk::RiskOutcome;
using qp::risk::python::PythonRiskGate;

class PythonRiskGateTest : public ::testing::Test {
   protected:
    void SetUp() override { qp::test::python(); }
};

TEST_F(PythonRiskGateTest, CheckWithNoneCallbackRejects) {
    PythonRiskGate<> gate{py::none(), py::none()};
    auto             decision = gate.check(qp::Intent{});
    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}

TEST_F(PythonRiskGateTest, CheckReturnsPythonDecision) {
    py::module_ types = py::module_::import(qp::test::kTestTypesModule);
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "def cb(intent):\n"
        "    order = types.Order(id=7, exchange=intent.exchange, market=intent.market,\n"
        "                        symbol=intent.symbol, side=types.Side.Buy,\n"
        "                        qty=abs(intent.target_position))\n"
        "    return types.RiskDecision(outcome=types.RiskOutcome.Approved, order=order)\n",
        locals);

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
    py::exec("cb = lambda intent: None\n", locals);

    PythonRiskGate<> gate{locals["cb"], py::none()};
    auto             decision = gate.check(qp::Intent{});
    EXPECT_EQ(decision.outcome, RiskOutcome::Rejected);
    EXPECT_FALSE(decision.order.has_value());
}

TEST_F(PythonRiskGateTest, OnTickReturnsOrders) {
    py::module_ types = py::module_::import(qp::test::kTestTypesModule);
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
        locals);

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
    py::exec("cb = lambda: None\n", locals);

    PythonRiskGate<> gate{py::none(), locals["cb"]};
    EXPECT_TRUE(gate.on_tick().empty());
}
