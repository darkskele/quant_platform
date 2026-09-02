#pragma once
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
/// between runs. carry's symbol/venue fields
/// are filled in by make_engine(); only the threshold/size fields are set.
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
/// timestamp order into the carry strategy behind a basic risk gate. The
/// dataset (root dir, symbol, inclusive day range) is chosen per run.
class FundingCarryBacktest : public BacktestBase<FundingCarryBacktest> {
   public:
    using Recorder = engine::EquitySeriesRecorder;

    FundingCarryBacktest(const std::filesystem::path& data_dir, std::string_view symbol,
                         std::chrono::year_month_day first_day,
                         std::chrono::year_month_day last_day)
        : symbol_id_(resolve_symbol(symbol)),
          futures_source_(config::make_futures_source(data_dir, symbol, first_day, last_day)),
          spot_source_(config::make_spot_source(data_dir, symbol, first_day, last_day)) {}

    void set_carry_config(const strategy::carry::Config& carry) { config_.carry = carry; }

    void set_risk_config(const risk::basic::BasicRiskGateConfig& risk) { config_.risk = risk; }

    auto sources() {
        return std::tuple<config::FuturesSource&, config::SpotSource&>{futures_source_,
                                                                       spot_source_};
    }

    std::tuple<config::Sink, config::Sink>& sinks() { return sinks_; }

    config::EngineType<Recorder> make_engine() {
        strategy::carry::Config carry = config_.carry;
        carry.symbol                  = symbol_id_;
        carry.futures_venue           = kFuturesVenue;
        carry.spot_venue              = kSpotVenue;

        risk::basic::BasicRiskGateConfig risk = config_.risk;
        risk.tracked = {{symbol_id_, kSpotVenue}, {symbol_id_, kFuturesVenue}};

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
            .final_spot_position    = portfolio_.position(symbol_id_, kSpotVenue),
            .final_futures_position = portfolio_.position(symbol_id_, kFuturesVenue),
            .equity_series          = equity_collector_.series(),
        };
    }

   private:
    // Runtime lookup into the compile-time symbol universe. Throws rather
    // than return a sentinel so a bad symbol fails the run, not silently.
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
    Config                                 config_{};
    engine::EquitySeriesCollector          equity_collector_{};
};

}  // namespace qp::backtest::funding_carry
