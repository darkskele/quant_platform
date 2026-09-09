#pragma once

// Engine's own swappable axes, same one-macro-per-combo convention as
// source_config.hpp/sink_config.hpp. Clock (always SimClock) and Transport
// (always BacktestInProcessTransport) aren't here: neither has a second
// implementation in a backtest, so there's no combo to select between yet
// (seams first, generality later). They're named directly in compose.hpp.

#if defined(QP_MATCHER_LAST_TRADE)
#include <filesystem>

#include "matcher/last_trade/last_trade_matcher.hpp"

namespace qp::backtest::config {
template <class Book>
using MatcherType = execution::sim::matcher::last_trade::LastTradeMatcher<Book>;

/// LastTradeMatcher takes no config. cost_table_path is accepted for a
/// uniform call site with cost-aware variants; it is ignored here.
template <class Book>
inline MatcherType<Book> make_matcher(const std::filesystem::path& = {}) {
    return MatcherType<Book>{};
}
}  // namespace qp::backtest::config
#elif defined(QP_MATCHER_COST_AWARE)
#include <filesystem>
#include <utility>

#include "matcher/cost_aware/cost_aware_matcher.hpp"
#include "matcher/cost_aware/cost_model/half_spread_linear/cost_row_reader.hpp"
#include "matcher/cost_aware/cost_model/half_spread_linear/half_spread_linear.hpp"
#include "venues.hpp"  // FuturesTable: the SymbolTable used to resolve CSV rows

namespace qp::backtest::config {
namespace hsl = execution::sim::matcher::cost_aware::cost_model::half_spread_linear;

template <class Book>
using CostModelType = hsl::HalfSpreadLinearImpact<Book>;

template <class Book>
using MatcherType =
    execution::sim::matcher::cost_aware::CostAwareMatcher<Book, CostModelType<Book> >;

template <class Book>
inline MatcherType<Book> make_matcher(const std::filesystem::path& cost_table_path) {
    auto rows = hsl::read_cost_rows_csv<FuturesTable>(cost_table_path);
    return MatcherType<Book>{CostModelType<Book>{std::move(rows)}};
}
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
