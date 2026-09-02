#include <benchmark/benchmark.h>

#include <cstddef>

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

// Real end-to-end pipeline throughput.
void BM_BacktestPipeline_RunOneWeek(benchmark::State& state) {
    for (auto _ : state) {
        qp::backtest::funding_carry::FundingCarryBacktest backtest;
        auto                                              results = backtest.run();
        benchmark::DoNotOptimize(results);
    }
}

BENCHMARK(BM_BacktestPipeline_RunOneWeek)->Unit(benchmark::kMillisecond);

void BM_BacktestSources_ReplayOneWeek(benchmark::State& state) {
    std::size_t events = 0;
    for (auto _ : state) {
        auto futures = config::make_futures_source(config::data_dir(), config::kSymbol,
                                                   config::kFirstDay, config::kLastDay);
        auto spot = config::make_spot_source(config::data_dir(), config::kSymbol, config::kFirstDay,
                                             config::kLastDay);
        events    = drain(futures) + drain(spot);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * events));
}

BENCHMARK(BM_BacktestSources_ReplayOneWeek)->Unit(benchmark::kMillisecond);
