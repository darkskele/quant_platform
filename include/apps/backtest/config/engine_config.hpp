#pragma once

// Engine's own swappable axes, same one-macro-per-combo convention as
// source_config.hpp/sink_config.hpp. Clock (always SimClock) and Transport
// (always BacktestInProcessTransport) aren't here: neither has a second
// implementation in a backtest, so there's no combo to select between yet
// (seams first, generality later). They're named directly in compose.hpp.

#if defined(QP_MATCHER_LAST_TRADE)
#include "matcher/last_trade/last_trade_matcher.hpp"

namespace qp::backtest::config {
template <class Book>
using MatcherType = execution::sim::matcher::last_trade::LastTradeMatcher<Book>;
}  // namespace qp::backtest::config
#else
#error "define exactly one QP_MATCHER_* macro (see cmake/apps/backtest/CMakeLists.txt)"
#endif

#if defined(QP_RISK_BASIC)
#include "basic_risk_gate.hpp"

namespace qp::backtest::config {
template <class Book>
using RiskType = risk::basic::BasicRiskGate<Book>;
}  // namespace qp::backtest::config
#elif defined(QP_RISK_PYTHON)
#include "python_risk_gate.hpp"

namespace qp::backtest::config {
template <class>
using RiskType = risk::python::PythonRiskGate<>;
}  // namespace qp::backtest::config
#else
#error "define exactly one QP_RISK_* macro (see cmake/apps/backtest/CMakeLists.txt)"
#endif

#if defined(QP_STRATEGY_FUNDING_CARRY)
#include "funding_carry_strategy.hpp"

namespace qp::backtest::config {
template <class Book>
using StrategyType = strategy::carry::FundingCarryStrategy<Book>;
}  // namespace qp::backtest::config
#elif defined(QP_STRATEGY_PYTHON)
#include "python_strategy.hpp"

namespace qp::backtest::config {
template <class>
using StrategyType = strategy::python::PythonStrategy<>;
}  // namespace qp::backtest::config
#else
#error "define exactly one QP_STRATEGY_* macro (see cmake/apps/backtest/CMakeLists.txt)"
#endif
