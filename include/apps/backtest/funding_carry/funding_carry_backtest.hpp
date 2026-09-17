#pragma once
#include <cstddef>
#include <functional>
#include <span>
#include <tuple>
#include <utility>

#include "backtest_base.hpp"
#include "backtest_in_process_transport.hpp"
#include "engine.hpp"
#include "eof_source.hpp"
#include "fanout_sink.hpp"
#include "funding_carry_strategy.hpp"
#include "recorder/equity_series/equity_series_recorder.hpp"
#include "sim_execution.hpp"
#include "subscription.hpp"
#include "types.hpp"

namespace qp::backtest::funding_carry {

inline constexpr std::size_t kRingCapacity = 1024;
inline constexpr std::size_t kNumLegs      = 2;

struct Config {
    strategy::carry::Config carry{};
};

struct Results {
    Notional                             final_cash{};
    Notional                             final_equity{};
    Qty                                  final_spot_position{};
    Qty                                  final_futures_position{};
    std::span<const engine::EquityPoint> equity_series{};
};

// @todo sources() is an EofSource stub until binary+zstd BinanceHistoricalSource lands.
template <class Matcher, class Risk, class RiskConfig>
class FundingCarryBacktest : public BacktestBase<FundingCarryBacktest<Matcher, Risk, RiskConfig>> {
   public:
    using Recorder    = engine::EquitySeriesRecorder;
    using Strategy    = strategy::carry::FundingCarryStrategy;
    using Exec        = execution::sim::SimExecution<Matcher>;
    using Sink        = data_source::sink::fanout::FanoutSink<kRingCapacity, 1>;
    using Tx          = engine::transport::BacktestInProcessTransport<kRingCapacity, kNumLegs>;
    using EngineT     = engine::Engine<Tx, Exec, Risk, Strategy, Recorder>;
    using MakeMatcher = std::function<Matcher(Portfolio&, const Subscription&)>;

    FundingCarryBacktest(Subscription subscription, Subscription::Instrument futures_leg,
                         Subscription::Instrument spot_leg, MakeMatcher make_matcher)
        : subscription_(std::move(subscription)),
          futures_leg_(futures_leg),
          spot_leg_(spot_leg),
          portfolio_(subscription_),
          make_matcher_(std::move(make_matcher)) {}

    void set_carry_config(const strategy::carry::Config& c) { config_.carry = c; }

    void set_risk_config(RiskConfig r) { risk_config_ = std::move(r); }

    auto sources() {
        return std::tuple<data_source::source::EofSource, data_source::source::EofSource>{};
    }

    std::tuple<Sink, Sink>& sinks() { return sinks_; }

    EngineT make_engine() {
        strategy::carry::Config carry = config_.carry;
        carry.futures = {futures_leg_.exchange, futures_leg_.market, futures_leg_.symbol};
        carry.spot    = {spot_leg_.exchange, spot_leg_.market, spot_leg_.symbol};

        equity_collector_.start();

        Tx       transport({&std::get<0>(sinks_).queue(), &std::get<1>(sinks_).queue()}, {0, 0});
        Risk     risk_gate{risk_config_, portfolio_};
        Strategy strategy{carry, portfolio_};
        Exec     exec{portfolio_, make_matcher_(portfolio_, subscription_)};
        return EngineT{std::move(transport), std::move(exec), std::move(risk_gate),
                       std::move(strategy),  portfolio_,      Recorder{&equity_collector_}};
    }

    Results results() {
        equity_collector_.finish();
        return Results{
            .final_cash   = portfolio_.cash(),
            .final_equity = portfolio_.equity(),
            .final_spot_position =
                portfolio_.position(spot_leg_.exchange, spot_leg_.market, spot_leg_.symbol),
            .final_futures_position = portfolio_.position(futures_leg_.exchange,
                                                          futures_leg_.market, futures_leg_.symbol),
            .equity_series          = equity_collector_.series(),
        };
    }

    const Subscription& subscription() const { return subscription_; }

    const Portfolio& portfolio() const { return portfolio_; }

    Subscription::Instrument futures_leg() const { return futures_leg_; }

    Subscription::Instrument spot_leg() const { return spot_leg_; }

   private:
    Subscription                  subscription_;
    Subscription::Instrument      futures_leg_;
    Subscription::Instrument      spot_leg_;
    std::tuple<Sink, Sink>        sinks_;
    Portfolio                     portfolio_;
    Config                        config_{};
    RiskConfig                    risk_config_{};
    engine::EquitySeriesCollector equity_collector_{};
    MakeMatcher                   make_matcher_;
};

}  // namespace qp::backtest::funding_carry
