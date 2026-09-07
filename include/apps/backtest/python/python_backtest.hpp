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

    PythonBacktest(const std::filesystem::path& data_dir, std::string_view symbol,
                   std::chrono::year_month_day first_day, std::chrono::year_month_day last_day)
        : symbol_id_(resolve_symbol(symbol)),
          futures_source_(config::make_futures_source(data_dir, symbol, first_day, last_day)),
          spot_source_(config::make_spot_source(data_dir, symbol, first_day, last_day)) {}

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
        return config::EngineType<Recorder>{
            std::move(transport),
            SimClock{},
            config::Exec{},
            risk::python::PythonRiskGate<>{check_, on_tick_},
            strategy::python::PythonStrategy<>{on_event_, on_timer_},
            portfolio_,
            Recorder{&equity_collector_}};
    }

    struct Results {
        Notional                             final_cash{};
        Notional                             final_equity{};
        SymbolId                             symbol_id{};
        std::span<const engine::EquityPoint> equity_series{};
    };

    Results results() {
        equity_collector_.finish();
        return Results{
            .final_cash    = portfolio_.cash(),
            .final_equity  = portfolio_.equity(),
            .symbol_id     = symbol_id_,
            .equity_series = equity_collector_.series(),
        };
    }

    const config::Book& portfolio() const { return portfolio_; }

   private:
    static SymbolId resolve_symbol(std::string_view symbol) {
        auto id = config::FuturesTable::id_of(symbol);
        if (!id) throw std::invalid_argument("unknown symbol: " + std::string(symbol));
        return *id;
    }

    SymbolId                               symbol_id_;
    config::FuturesSource                  futures_source_;
    config::SpotSource                     spot_source_;
    std::tuple<config::Sink, config::Sink> sinks_;
    config::Book                           portfolio_;
    pybind11::object                       on_event_{pybind11::none()};
    pybind11::object                       on_timer_{pybind11::none()};
    pybind11::object                       check_{pybind11::none()};
    pybind11::object                       on_tick_{pybind11::none()};
    engine::EquitySeriesCollector          equity_collector_{};
};

}  // namespace qp::backtest::python
