#include <benchmark/benchmark.h>

#include <string_view>

#include "agg_trades.hpp"
#include "endpoints.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::BinanceMarket;
using binance::endpoint;
using binance::EndpointKind;
using binance::parsers::parse_agg_trades_row;
using qp::Timestamp;
using qp::TradeEvent;

namespace {

// A day of aggTrades is over a million rows, so this row is the one whose cost
// sets how fast a trade sweep can go. Read beside the kline row benches.
constexpr std::string_view kUsdMRow =
    "2742797403,105583.3,0.01,6362492256,6362492256,1748822400011,false";
constexpr std::string_view kSpotRow =
    "3587770505,105642.93000000,0.00047000,4976176577,4976176577,1748822400054776,False,True";

void BM_BinanceParser_AggTradesRowUsdM(benchmark::State& state) {
    const auto& entry = endpoint(BinanceMarket::UsdM, EndpointKind::AggTrades);
    Timestamp   ts{};
    TradeEvent  trade{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_agg_trades_row(entry, kUsdMRow, ts, trade));
        benchmark::DoNotOptimize(trade);
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_BinanceParser_AggTradesRowSpot(benchmark::State& state) {
    const auto& entry = endpoint(BinanceMarket::Spot, EndpointKind::AggTrades);
    Timestamp   ts{};
    TradeEvent  trade{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_agg_trades_row(entry, kSpotRow, ts, trade));
        benchmark::DoNotOptimize(trade);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinanceParser_AggTradesRowUsdM);
BENCHMARK(BM_BinanceParser_AggTradesRowSpot);

}  // namespace
