#pragma once
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

#include "backtest_base.hpp"
#include "basic_risk_gate.hpp"
#include "config/compose.hpp"
#include "funding_carry_strategy.hpp"
#include "recorder/equity_series/equity_series_recorder.hpp"
#include "sim_clock.hpp"
#include "types.hpp"

namespace qp::backtest::funding_carry {

// Venue indices, matching config::VenueTables' declaration order.
inline constexpr VenueId kFuturesVenue = 0;
inline constexpr VenueId kSpotVenue    = 1;

/// The runtime-swept knobs: exactly what a Python-side optimizer sets
/// between runs. Symbol/dataset/component types are all compile-time.
/// carry's symbol/venue fields are filled in by make_engine(); only the
/// threshold/size fields are set.
struct Config {
    strategy::carry::Config          carry{};
    risk::basic::BasicRiskGateConfig risk{};
};

/// Final snapshot plus the per-step equity series. Higher-level metrics
/// (Sharpe, drawdown curve) are computed downstream from the series.
struct Results {
    Notional                             final_cash{};
    Notional                             final_equity{};
    Qty                                  final_spot_position{};
    Qty                                  final_futures_position{};
    std::span<const engine::EquityPoint> equity_series{};
};

/// The funding-carry backtest variant: futures + spot CSV legs, merged in
/// timestamp order into the carry strategy behind a basic risk gate.
class FundingCarryBacktest : public BacktestBase<FundingCarryBacktest> {
   public:
    using Recorder = engine::EquitySeriesRecorder;

    FundingCarryBacktest()
        : futures_source_(config::make_futures_source(config::data_dir(), config::kSymbol,
                                                      config::kFirstDay, config::kLastDay)),
          spot_source_(config::make_spot_source(config::data_dir(), config::kSymbol,
                                                config::kFirstDay, config::kLastDay)) {}

    void set_carry_config(const strategy::carry::Config& carry) { config_.carry = carry; }

    void set_risk_config(const risk::basic::BasicRiskGateConfig& risk) { config_.risk = risk; }

    auto sources() {
        return std::tuple<config::FuturesSource&, config::SpotSource&>{futures_source_,
                                                                       spot_source_};
    }

    std::tuple<config::Sink, config::Sink>& sinks() { return sinks_; }

    config::EngineType<Recorder> make_engine() {
        strategy::carry::Config carry = config_.carry;
        carry.symbol                  = config::kSymbolId;
        carry.futures_venue           = kFuturesVenue;
        carry.spot_venue              = kSpotVenue;

        risk::basic::BasicRiskGateConfig risk = config_.risk;
        risk.tracked = {{config::kSymbolId, kSpotVenue}, {config::kSymbolId, kFuturesVenue}};

        equity_collector_.start();

        // Both sinks have NumConsumers == 1, so this Engine's consumer
        // index on each queue is trivially 0.
        config::Tx transport({&std::get<0>(sinks_).queue(), &std::get<1>(sinks_).queue()}, {0, 0});
        config::Risk     risk_gate{risk, portfolio_};
        config::Strategy strategy{carry, portfolio_};
        return config::EngineType<Recorder>{
            std::move(transport),        SimClock{},          config::Exec{},
            std::move(risk_gate),        std::move(strategy), portfolio_,
            Recorder{&equity_collector_}};
    }

    Results results() {
        equity_collector_.finish();
        return Results{
            .final_cash             = portfolio_.cash(),
            .final_equity           = portfolio_.equity(),
            .final_spot_position    = portfolio_.position(config::kSymbolId, kSpotVenue),
            .final_futures_position = portfolio_.position(config::kSymbolId, kFuturesVenue),
            .equity_series          = equity_collector_.series(),
        };
    }

   private:
    config::FuturesSource                  futures_source_;
    config::SpotSource                     spot_source_;
    std::tuple<config::Sink, config::Sink> sinks_;
    config::Book                           portfolio_;
    Config                                 config_{};
    engine::EquitySeriesCollector          equity_collector_{};
};

}  // namespace qp::backtest::funding_carry
