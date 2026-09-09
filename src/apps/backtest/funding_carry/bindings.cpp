#include <pybind11/pybind11.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>

#include "funding_carry/funding_carry_backtest.hpp"

namespace py     = pybind11;
namespace config = qp::backtest::config;

namespace {

using CarryConfig = qp::strategy::carry::Config;
using RiskConfig  = qp::risk::basic::BasicRiskGateConfig;
using EquityPoint = qp::engine::EquityPoint;

// One dataset the notebook picks at runtime: where the CSVs live, which
// symbol, and the inclusive day range. Built once, reused across a config
// sweep. Dates are "YYYY-MM-DD"; a bad one fails construction.
// cost_table_path is used only when the matcher is cost-aware. Empty is fine
// for LastTradeMatcher builds.
struct Dataset {
    std::filesystem::path       data_dir;
    std::string                 symbol;
    std::chrono::year_month_day first_day;
    std::chrono::year_month_day last_day;
    std::filesystem::path       cost_table_path;

    Dataset(std::string dir, std::string sym, const std::string& first, const std::string& last,
            std::string cost_path = "")
        : data_dir(std::move(dir)), symbol(std::move(sym)), cost_table_path(std::move(cost_path)) {
        auto f = config::parse_day(first);
        auto l = config::parse_day(last);
        if (!f) throw std::invalid_argument("bad first_day: " + first);
        if (!l) throw std::invalid_argument("bad last_day: " + last);
        first_day = *f, last_day = *l;
    }
};

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
    double sd             = std::sqrt(var);
    double span_ns        = static_cast<double>(pts.back().ts - pts.front().ts);
    double steps_per_year = span_ns > 0.0 ? (pts.size() - 1) / (span_ns / kNsPerYear) : 0.0;

    out["pnl"]          = pnl;
    out["max_drawdown"] = mdd;
    out["sharpe"]       = sd > 0.0 ? (mean / sd) * std::sqrt(steps_per_year) : 0.0;
    return out;
}

// The backtest owns the buffer Results::equity_series views, so the results
// must be consumed while it is still alive. Returning them out of here would
// dangle the span: a use-after-free that only bites once the freed buffer is
// large enough to be unmapped.
template <class Consume>
auto with_run(const Dataset& ds, const CarryConfig& carry, const RiskConfig& risk,
              Consume&& consume) {
    qp::backtest::funding_carry::FundingCarryBacktest bt{ds.data_dir, ds.symbol, ds.first_day,
                                                         ds.last_day, ds.cost_table_path};
    bt.set_carry_config(carry);
    bt.set_risk_config(risk);
    return consume(bt.run());
}

}  // namespace

// Component types stay compile-time. The dataset (root dir, symbol, day
// range) and the swept Config are runtime: one build serves every symbol.
PYBIND11_MODULE(qp_backtest, m) {
    m.doc() = "Funding-carry backtest: pick a Dataset, sweep the Config, get metrics or a series.";

    py::class_<Dataset>(m, "Dataset")
        .def(py::init<std::string, std::string, std::string, std::string, std::string>(),
             py::arg("data_dir"), py::arg("symbol"), py::arg("first_day"), py::arg("last_day"),
             py::arg("cost_table_path") = "");

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
        [](const Dataset& dataset, CarryConfig carry, RiskConfig risk, double train_fraction) {
            return with_run(dataset, carry, risk, [&](const auto& r) {
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
            });
        },
        py::arg("dataset"), py::arg("carry") = CarryConfig{}, py::arg("risk") = RiskConfig{},
        py::arg("train_fraction") = 0.7,
        "Run and return scalar train/validation metrics (no full series).");

    // Plotting path: the equity curve, downsampled by stride to keep the
    // transfer and the plot cheap.
    m.def(
        "run_series",
        [](const Dataset& dataset, CarryConfig carry, RiskConfig risk, std::size_t stride) {
            return with_run(dataset, carry, risk, [&](const auto& r) {
                if (stride < 1) stride = 1;
                py::list   series;
                const auto s = r.equity_series;
                for (std::size_t i = 0; i < s.size(); i += stride) series.append(s[i]);
                return series;
            });
        },
        py::arg("dataset"), py::arg("carry") = CarryConfig{}, py::arg("risk") = RiskConfig{},
        py::arg("stride") = 1, "Run and return the equity series, taking every stride-th point.");
}
