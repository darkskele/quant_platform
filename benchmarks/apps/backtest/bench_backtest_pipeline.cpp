#include <benchmark/benchmark.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include "config/compose.hpp"
#include "funding_carry/funding_carry_backtest.hpp"
#include "source.hpp"

namespace {

namespace config = qp::backtest::config;
using qp::data_source::source::SourceStatus;

template <class Source>
std::size_t drain(Source& source) {
    std::size_t n = 0;
    for (;;) {
        auto r = source.next();
        if (r) {
            ++n;
            benchmark::DoNotOptimize(r);
        } else if (r.error() == SourceStatus::Eof) {
            break;
        }
    }
    return n;
}

}  // namespace

// The committed fixture, passed at runtime the way a real run picks a dataset.
const std::filesystem::path       kDataDir   = QP_BACKTEST_DATA_DIR;
constexpr std::string_view        kSymbol    = QP_BACKTEST_SYMBOL;
const std::chrono::year_month_day kFirstDay  = *config::parse_day(QP_BACKTEST_FIRST_DAY);
const std::chrono::year_month_day kLastDay   = *config::parse_day(QP_BACKTEST_LAST_DAY);
const std::filesystem::path       kCostTable = QP_BACKTEST_COST_TABLE;

// Real end-to-end pipeline throughput.
void BM_BacktestPipeline_RunOneWeek(benchmark::State& state) {
    for (auto _ : state) {
        qp::backtest::funding_carry::FundingCarryBacktest backtest{kDataDir, kSymbol, kFirstDay,
                                                                   kLastDay, kCostTable};
        auto                                              results = backtest.run();
        benchmark::DoNotOptimize(results);
    }
}

BENCHMARK(BM_BacktestPipeline_RunOneWeek)->Unit(benchmark::kMillisecond);

void BM_BacktestSources_ReplayOneWeek(benchmark::State& state) {
    std::size_t events = 0;
    for (auto _ : state) {
        auto futures = config::make_futures_source(kDataDir, kSymbol, kFirstDay, kLastDay);
        auto spot    = config::make_spot_source(kDataDir, kSymbol, kFirstDay, kLastDay);
        events       = drain(futures) + drain(spot);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * events));
}

BENCHMARK(BM_BacktestSources_ReplayOneWeek)->Unit(benchmark::kMillisecond);
