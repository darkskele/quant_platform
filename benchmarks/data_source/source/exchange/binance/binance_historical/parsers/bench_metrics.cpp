#include <benchmark/benchmark.h>

#include <string_view>

#include "metrics.hpp"
#include "types.hpp"

using qp::OpenInterestEvent;
using qp::Timestamp;
using qp::data_source::source::exchange::binance::parsers::parse_metrics_row;

namespace {

// Real rows. The Coin-M case walks three blank fields, so the two are timed
// apart rather than averaged into one number.
constexpr std::string_view kUsdMRow =
    "2025-06-02 00:00:00,BTCUSDT,82795.1230000000000000,8742808969.7711000000000000,1.21422318,"
    "1.53311800,1.13511948,0.94273600";

constexpr std::string_view kCoinMRow =
    "2025-06-02 00:00:00,BTCUSD_PERP,23906290.00000000,22626.52258397,,,,0.52745930";

void BM_BinanceParser_MetricsRowUsdM(benchmark::State& state) {
    Timestamp         ts{};
    OpenInterestEvent metrics{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_metrics_row(kUsdMRow, ts, metrics));
        benchmark::DoNotOptimize(metrics);
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_BinanceParser_MetricsRowCoinM(benchmark::State& state) {
    Timestamp         ts{};
    OpenInterestEvent metrics{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_metrics_row(kCoinMRow, ts, metrics));
        benchmark::DoNotOptimize(metrics);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinanceParser_MetricsRowUsdM);
BENCHMARK(BM_BinanceParser_MetricsRowCoinM);

}  // namespace
