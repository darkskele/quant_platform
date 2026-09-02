#include <pybind11/pybind11.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

#include "funding_carry/funding_carry_backtest.hpp"

namespace py = pybind11;

namespace {

using CarryConfig = qp::strategy::carry::Config;
using RiskConfig  = qp::risk::basic::BasicRiskGateConfig;
using EquityPoint = qp::engine::EquityPoint;

constexpr double kNsPerYear = 365.25 * 24 * 60 * 60 * 1e9;

// PnL, worst peak-to-trough drawdown, and a rough annualized Sharpe over one
// window of the equity curve. Computed here so a tuning run never has to ship
// a million per-point objects across the boundary.
py::dict window_metrics(std::span<const EquityPoint> pts) {
    py::dict out;
    if (pts.size() < 2) {
        out["pnl"] = 0.0, out["max_drawdown"] = 0.0, out["sharpe"] = 0.0;
        return out;
    }

    double pnl  = pts.back().equity - pts.front().equity;
    double peak = pts.front().equity;
    double mdd  = 0.0;
    for (const auto& p : pts) {
        peak = std::max(peak, p.equity);
        mdd  = std::max(mdd, peak - p.equity);
    }

    double mean = pnl / static_cast<double>(pts.size() - 1);
    double var  = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        double d = (pts[i].equity - pts[i - 1].equity) - mean;
        var += d * d;
    }
    var /= static_cast<double>(pts.size() - 1);
    double sd            = std::sqrt(var);
    double span_ns       = static_cast<double>(pts.back().ts - pts.front().ts);
    double steps_per_year = span_ns > 0.0 ? (pts.size() - 1) / (span_ns / kNsPerYear) : 0.0;

    out["pnl"]          = pnl;
    out["max_drawdown"] = mdd;
    out["sharpe"]       = sd > 0.0 ? (mean / sd) * std::sqrt(steps_per_year) : 0.0;
    return out;
}

qp::backtest::funding_carry::Results run(const CarryConfig& carry, const RiskConfig& risk,
                                         qp::backtest::funding_carry::FundingCarryBacktest& bt) {
    bt.set_carry_config(carry);
    bt.set_risk_config(risk);
    return bt.run();
}

}  // namespace

// Only the runtime-swept knobs are exposed. Symbol, dataset, date range, and
// component types are compile-time, baked in by the target's defines.
PYBIND11_MODULE(qp_backtest, m) {
    m.doc() = "Funding-carry backtest: sweep the runtime Config, get metrics or an equity series.";

    py::class_<CarryConfig>(m, "CarryConfig")
        .def(py::init<>())
        .def_readwrite("target_qty", &CarryConfig::target_qty)
        .def_readwrite("entry_funding_rate", &CarryConfig::entry_funding_rate)
        .def_readwrite("exit_funding_rate", &CarryConfig::exit_funding_rate);

    py::class_<RiskConfig>(m, "RiskConfig")
        .def(py::init<>())
        .def_readwrite("max_position_qty", &RiskConfig::max_position_qty)
        .def_readwrite("max_drawdown", &RiskConfig::max_drawdown);

    py::class_<EquityPoint>(m, "EquityPoint")
        .def_readonly("ts", &EquityPoint::ts)
        .def_readonly("equity", &EquityPoint::equity);

    // Fast path for tuning: run, split the equity series at train_fraction,
    // return scalar metrics per window. No per-point objects cross over.
    m.def(
        "run_metrics",
        [](CarryConfig carry, RiskConfig risk, double train_fraction) {
            qp::backtest::funding_carry::FundingCarryBacktest bt;
            auto        r     = run(carry, risk, bt);
            auto        s     = r.equity_series;
            std::size_t split = static_cast<std::size_t>(s.size() * train_fraction);

            py::dict out;
            out["final_cash"]             = r.final_cash;
            out["final_equity"]           = r.final_equity;
            out["final_spot_position"]    = r.final_spot_position;
            out["final_futures_position"] = r.final_futures_position;
            out["points"]                 = s.size();
            out["train"]                  = window_metrics(s.subspan(0, split));
            out["val"]                    = window_metrics(s.subspan(split));
            return out;
        },
        py::arg("carry") = CarryConfig{}, py::arg("risk") = RiskConfig{},
        py::arg("train_fraction") = 0.7,
        "Run and return scalar train/validation metrics (no full series).");

    // Plotting path: the equity curve, downsampled by stride to keep the
    // transfer and the plot cheap.
    m.def(
        "run_series",
        [](CarryConfig carry, RiskConfig risk, std::size_t stride) {
            qp::backtest::funding_carry::FundingCarryBacktest bt;
            auto r = run(carry, risk, bt);
            if (stride < 1) stride = 1;
            py::list series;
            const auto s = r.equity_series;
            for (std::size_t i = 0; i < s.size(); i += stride) series.append(s[i]);
            return series;
        },
        py::arg("carry") = CarryConfig{}, py::arg("risk") = RiskConfig{}, py::arg("stride") = 1,
        "Run and return the equity series, taking every stride-th point.");
}
