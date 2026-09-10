#pragma once
#include <pybind11/pybind11.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "backtest_base.hpp"
#include "config/compose.hpp"
#include "python_risk_gate.hpp"
#include "python_strategy.hpp"
#include "recorder/equity_series/equity_series_recorder.hpp"
#include "sim_clock.hpp"
#include "types.hpp"

namespace qp::backtest::python {

/// Backtest variant whose Strategy and RiskGate forward to python callbacks
/// set from the notebook. Each run reconstructs the adapters around the
/// current callback slots.
class PythonBacktest : public BacktestBase<PythonBacktest> {
   public:
    using Recorder = engine::EquitySeriesRecorder;

    PythonBacktest(const std::filesystem::path& data_dir, const std::vector<std::string>& symbols,
                   std::chrono::year_month_day first_day, std::chrono::year_month_day last_day,
                   std::filesystem::path cost_table_path = {})
        : symbol_ids_(resolve_symbols(symbols)),
          futures_source_(config::make_futures_source(data_dir, symbols, first_day, last_day)),
          spot_source_(config::make_spot_source(data_dir, symbols, first_day, last_day)),
          cost_table_path_(std::move(cost_table_path)) {}

    void set_on_event(pybind11::object cb) { on_event_ = std::move(cb); }

    void set_on_timer(pybind11::object cb) { on_timer_ = std::move(cb); }

    void set_check(pybind11::object cb) { check_ = std::move(cb); }

    void set_on_tick(pybind11::object cb) { on_tick_ = std::move(cb); }

    auto sources() {
        return std::tuple<config::FuturesSource&, config::SpotSource&>{futures_source_,
                                                                       spot_source_};
    }

    std::tuple<config::Sink, config::Sink>& sinks() { return sinks_; }

    config::EngineType<Recorder> make_engine() {
        equity_collector_.start();
        config::Tx transport({&std::get<0>(sinks_).queue(), &std::get<1>(sinks_).queue()}, {0, 0});
        // make_matcher is variation-specific: LastTrade ignores the path,
        // cost-aware reads the honest fee/spread/impact table from it.
        config::Exec exec{config::make_matcher<config::Book>(cost_table_path_)};
        return config::EngineType<Recorder>{
            std::move(transport),
            SimClock{},
            std::move(exec),
            risk::python::PythonRiskGate<>{check_, on_tick_},
            strategy::python::PythonStrategy<>{on_event_, on_timer_},
            portfolio_,
            Recorder{&equity_collector_}};
    }

    struct Results {
        Notional                             final_cash{};
        Notional                             final_equity{};
        std::vector<SymbolId>                symbol_ids{};
        std::span<const engine::EquityPoint> equity_series{};
    };

    Results results() {
        equity_collector_.finish();
        return Results{
            .final_cash    = portfolio_.cash(),
            .final_equity  = portfolio_.equity(),
            .symbol_ids    = symbol_ids_,
            .equity_series = equity_collector_.series(),
        };
    }

    const config::Book& portfolio() const { return portfolio_; }

   private:
    static std::vector<SymbolId> resolve_symbols(const std::vector<std::string>& symbols) {
        std::vector<SymbolId> ids;
        ids.reserve(symbols.size());
        for (const auto& symbol : symbols) {
            auto id = config::FuturesTable::id_of(symbol);
            if (!id) throw std::invalid_argument("unknown symbol: " + symbol);
            ids.push_back(*id);
        }
        return ids;
    }

    std::vector<SymbolId>                  symbol_ids_;
    config::FuturesSource                  futures_source_;
    config::SpotSource                     spot_source_;
    std::filesystem::path                  cost_table_path_;
    std::tuple<config::Sink, config::Sink> sinks_;
    config::Book                           portfolio_;
    pybind11::object                       on_event_{pybind11::none()};
    pybind11::object                       on_timer_{pybind11::none()};
    pybind11::object                       check_{pybind11::none()};
    pybind11::object                       on_tick_{pybind11::none()};
    engine::EquitySeriesCollector          equity_collector_{};
};

}  // namespace qp::backtest::python
