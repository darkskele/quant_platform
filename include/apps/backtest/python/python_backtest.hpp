#pragma once
#include <pybind11/pybind11.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include "backtest_base.hpp"
#include "backtest_in_process_transport.hpp"
#include "engine.hpp"
#include "fanout_sink.hpp"
#include "python_risk_gate.hpp"
#include "python_strategy.hpp"
#include "recorder/equity_series/equity_series_recorder.hpp"
#include "sim_execution.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace qp::backtest::python {

inline constexpr std::size_t kRingCapacity = 1024;

/// Everything a python backtest needs except where its events come from. A
/// variant derives from this and supplies sources().
///
/// @tparam Derived the concrete variant, which owns its sources.
/// @tparam Matcher the fill model.
template <class Derived, class Matcher>
class PythonBacktestBase : public BacktestBase<Derived> {
   public:
    using Recorder    = engine::EquitySeriesRecorder;
    using Risk        = risk::python::PythonRiskGate<>;
    using Strategy    = strategy::python::PythonStrategy<>;
    using Exec        = execution::sim::SimExecution<Matcher>;
    using Sink        = data_source::sink::fanout::FanoutSink<kRingCapacity, 1>;
    using Tx          = engine::transport::BacktestInProcessTransport<kRingCapacity, 1>;
    using EngineT     = engine::Engine<Tx, Exec, Risk, Strategy, Recorder>;
    using MakeMatcher = std::function<Matcher(Portfolio&, const Subscription&)>;

    void set_on_event(pybind11::object cb) { on_event_ = std::move(cb); }

    void set_on_timer(pybind11::object cb) { on_timer_ = std::move(cb); }

    void set_check(pybind11::object cb) { check_ = std::move(cb); }

    void set_on_tick(pybind11::object cb) { on_tick_ = std::move(cb); }

    void set_timer_period(Timestamp period_ns) { timer_period_ns_ = period_ns; }

    void set_event_kinds(const std::vector<EventKind>& kinds) {
        if (kinds.empty()) {
            event_kind_mask_ = strategy::python::kAllKinds;
            return;
        }
        strategy::python::KindMask mask = 0;
        for (EventKind k : kinds) mask |= strategy::python::kind_bit(k);
        event_kind_mask_ = mask;
    }

    Notional equity() const { return portfolio_.equity(); }

    Notional cash() const { return portfolio_.cash(); }

    Qty position(std::uint16_t exchange, std::uint16_t market, std::uint16_t symbol) const {
        return portfolio_.position(exchange, market, symbol);
    }

    Price mark(std::uint16_t exchange, std::uint16_t market, std::uint16_t symbol) const {
        return portfolio_.mark(exchange, market, symbol);
    }

    std::tuple<Sink>& sinks() { return sinks_; }

    EngineT make_engine() {
        // run() releases the GIL for the threaded section, but building the
        // engine copies python objects into the strategy and the risk gate,
        // which is a refcount touch and needs it held.
        pybind11::gil_scoped_acquire gil;
        equity_collector_.start();
        Tx   transport({&std::get<0>(sinks_).queue()}, {0}, timer_period_ns_);
        Exec exec{portfolio_, make_matcher_(portfolio_, subscription_)};
        return EngineT{
            std::move(transport),
            std::move(exec),
            Risk{check_, on_tick_},
            Strategy{on_event_, on_timer_, event_kind_mask_},
            portfolio_,
            Recorder{&equity_collector_, kEquitySampleIntervalNs},
        };
    }

    struct Results {
        Notional                              final_cash{};
        Notional                              final_equity{};
        std::vector<Subscription::Instrument> instruments{};
        std::vector<Notional>                 fees{};
        std::vector<Notional>                 funding_received{};
        std::vector<Notional>                 funding_paid{};
        std::vector<Notional>                 basis_pnl{};
        std::span<const engine::EquityPoint>  equity_series{};
    };

    Results results() {
        equity_collector_.finish();
        Results r{
            .final_cash    = portfolio_.cash(),
            .final_equity  = portfolio_.equity(),
            .instruments   = instruments_,
            .equity_series = equity_collector_.series(),
        };
        for (const auto& inst : instruments_) {
            r.fees.push_back(portfolio_.fees(inst.exchange, inst.market, inst.symbol));
            r.funding_received.push_back(
                portfolio_.funding_received(inst.exchange, inst.market, inst.symbol));
            r.funding_paid.push_back(
                portfolio_.funding_paid(inst.exchange, inst.market, inst.symbol));
            r.basis_pnl.push_back(portfolio_.position(inst.exchange, inst.market, inst.symbol) *
                                  portfolio_.mark(inst.exchange, inst.market, inst.symbol));
        }
        return r;
    }

    const Portfolio& portfolio() const { return portfolio_; }

    const Subscription& subscription() const { return subscription_; }

   protected:
    /// Protected, so the base cannot be built on its own. A variant has to
    /// exist to supply sources().
    PythonBacktestBase(Subscription subscription, MakeMatcher make_matcher)
        : subscription_(std::move(subscription)),
          instruments_(all_instruments(subscription_)),
          portfolio_(subscription_),
          make_matcher_(std::move(make_matcher)) {}

   private:
    static std::vector<Subscription::Instrument> all_instruments(const Subscription& sub) {
        std::vector<Subscription::Instrument> out;
        for (const auto& ex : sub.exchanges())
            for (const auto& mkt : ex.markets)
                for (std::size_t i = 0; i < mkt.symbols.size(); ++i)
                    out.push_back({.exchange = ex.exchange,
                                   .market   = mkt.market,
                                   .symbol   = static_cast<std::uint16_t>(i)});
        return out;
    }

    Subscription                          subscription_;
    std::vector<Subscription::Instrument> instruments_;
    std::tuple<Sink>                      sinks_;
    Portfolio                             portfolio_;
    pybind11::object                      on_event_{pybind11::none()};
    pybind11::object                      on_timer_{pybind11::none()};
    pybind11::object                      check_{pybind11::none()};
    pybind11::object                      on_tick_{pybind11::none()};
    Timestamp                             timer_period_ns_{0};
    strategy::python::KindMask            event_kind_mask_{strategy::python::kAllKinds};
    engine::EquitySeriesCollector         equity_collector_{};
    MakeMatcher                           make_matcher_;

    static constexpr Timestamp kEquitySampleIntervalNs = 24LL * 60 * 60 * 1'000'000'000;
};

}  // namespace qp::backtest::python
