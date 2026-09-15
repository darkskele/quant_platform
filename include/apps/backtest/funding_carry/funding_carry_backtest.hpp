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
#include "types.hpp"

namespace qp::backtest::funding_carry {

inline constexpr Market kFuturesMarket = Market::BinanceUsdm;
inline constexpr Market kSpotMarket    = Market::BinanceSpot;

struct Config {
    strategy::carry::Config          carry{};
    risk::basic::BasicRiskGateConfig risk{};
};

/// Final snapshot plus the per-step equity series. 
struct Results {
    Notional                             final_cash{};
    Notional                             final_equity{};
    Qty                                  final_spot_position{};
    Qty                                  final_futures_position{};
    std::span<const engine::EquityPoint> equity_series{};
};

/// The funding-carry backtest variant.
class FundingCarryBacktest : public BacktestBase<FundingCarryBacktest> {
   public:
    using Recorder = engine::EquitySeriesRecorder;

    FundingCarryBacktest(const std::filesystem::path& data_dir, std::string_view symbol,
                         std::chrono::year_month_day first_day,
                         std::chrono::year_month_day last_day,
                         std::filesystem::path       cost_table_path = {})
        : symbol_id_(resolve_symbol(symbol)),
          futures_source_(config::make_futures_source(data_dir, symbol, first_day, last_day)),
          spot_source_(config::make_spot_source(data_dir, symbol, first_day, last_day)),
          portfolio_(config::make_subscription()),
          cost_table_path_(std::move(cost_table_path)) {}

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
        carry.futures_market          = kFuturesMarket;
        carry.spot_market             = kSpotMarket;

        risk::basic::BasicRiskGateConfig risk = config_.risk;
        risk.tracked = {{symbol_id_, kSpotMarket}, {symbol_id_, kFuturesMarket}};

        equity_collector_.start();

        // Both sinks have NumConsumers == 1, so this Engine's consumer
        // index on each queue is trivially 0.
        config::Tx transport({&std::get<0>(sinks_).queue(), &std::get<1>(sinks_).queue()}, {0, 0});
        config::Risk     risk_gate{risk, portfolio_};
        config::Strategy strategy{carry, portfolio_};
        // make_matcher is variation-specific: LastTrade ignores the path,
        // cost-aware reads it.
        config::Exec exec{portfolio_, config::make_matcher(portfolio_, cost_table_path_)};
        return config::EngineType<Recorder>{std::move(transport), std::move(exec),
                                            std::move(risk_gate), std::move(strategy),
                                            portfolio_,           Recorder{&equity_collector_}};
    }

    Results results() {
        equity_collector_.finish();
        return Results{
            .final_cash             = portfolio_.cash(),
            .final_equity           = portfolio_.equity(),
            .final_spot_position    = portfolio_.position(symbol_id_, kSpotMarket),
            .final_futures_position = portfolio_.position(symbol_id_, kFuturesMarket),
            .equity_series          = equity_collector_.series(),
        };
    }

   private:
    // Runtime lookup into the compile-time symbol universe.
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
    std::filesystem::path                  cost_table_path_;
    Config                                 config_{};
    engine::EquitySeriesCollector          equity_collector_{};
};

}  // namespace qp::backtest::funding_carry
