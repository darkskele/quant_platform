#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "python_strategy.hpp"
#include "support/python_interpreter.hpp"
#include "types.hpp"

namespace py = pybind11;
using qp::strategy::python::kAllKinds;
using qp::strategy::python::kind_bit;
using qp::strategy::python::KindMask;
using qp::strategy::python::PythonStrategy;

class PythonStrategyTest : public ::testing::Test {
   protected:
    void SetUp() override { qp::test::python(); }
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
    py::module_ types = py::module_::import(qp::test::kTestTypesModule);
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "def cb(ts):\n"
        "    return [\n"
        "        types.Intent(exchange=0, market=2, symbol=1, target_position=3.5),\n"
        "        types.Intent(exchange=0, market=5, symbol=4, target_position=-6.0),\n"
        "    ]\n",
        locals);

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
    py::exec("cb = lambda t: None\n", locals);

    PythonStrategy<> strategy{py::none(), locals["cb"]};
    EXPECT_TRUE(strategy.on_timer(0).empty());
}

TEST_F(PythonStrategyTest, BufferIsReusedAcrossCalls) {
    py::module_ types = py::module_::import(qp::test::kTestTypesModule);
    py::dict    locals;
    locals["types"] = types;
    py::exec(
        "state = [0]\n"
        "def cb(ts):\n"
        "    state[0] += 1\n"
        "    return [types.Intent(exchange=0, market=0, symbol=state[0], "
        "target_position=float(state[0]))]\n",
        locals);

    PythonStrategy<> strategy{py::none(), locals["cb"]};

    auto first = strategy.on_timer(0);
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first[0].symbol, 1u);

    auto second = strategy.on_timer(1);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(second[0].symbol, 2u);
}

// The mask used to be eight bits, so the ninth kind shifted off the top and was
// dropped even under the all kinds default. Every kind has to keep its own bit.
TEST(PythonStrategyKindMask, EveryKindHasItsOwnBitInsideAllKinds) {
    KindMask seen = 0;
    for (std::size_t k = 0; k < qp::kEventKindCount; ++k) {
        const auto bit = kind_bit(static_cast<qp::EventKind>(k));
        EXPECT_NE(bit, 0u) << "kind " << k;
        EXPECT_NE(kAllKinds & bit, 0u) << "kind " << k;
        EXPECT_EQ(seen & bit, 0u) << "kind " << k << " shares a bit";
        seen |= bit;
    }
}

TEST_F(PythonStrategyTest, MaskedOutKindNeverReachesTheCallback) {
    py::dict locals;
    py::exec("def cb(event):\n    raise RuntimeError('should have been filtered')\n", locals);

    PythonStrategy<> strategy{locals["cb"], py::none(), kind_bit(qp::EventKind::Trade)};
    qp::MarketEvent  evt;
    evt.base = {.kind = qp::EventKind::BookDepth, .ts = 0};
    EXPECT_TRUE(strategy.on_event(evt).empty());
}
