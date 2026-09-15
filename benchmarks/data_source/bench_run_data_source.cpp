#include <benchmark/benchmark.h>

#include <array>
#include <tuple>

#include "control_channel.hpp"
#include "markets.hpp"
#include "run_data_source.hpp"
#include "source.hpp"
#include "types.hpp"

using namespace qp;
using qp::data_source::run_data_source;
using qp::data_source::source::PullResult;
using qp::data_source::source::SourceStatus;

namespace {

constexpr int                   kRounds = 1000;
constexpr std::array<Market, 1> kLegs1{Market::BinanceUsdm};
constexpr std::array<Market, 4> kLegs4{Market::BinanceUsdm, Market::BinanceCoinm,
                                       Market::BinanceSpot, Market::BinanceUsdm};

struct CountingSource {
    int remaining;

    PullResult next() {
        if (remaining <= 0) return std::unexpected(SourceStatus::Eof);
        --remaining;
        return MarketEvent{TradeEvent{}};
    }
};

struct CountingSink {
    long count = 0;

    bool record(MarketEvent&&) {
        ++count;
        return true;
    }
};

void BM_RunDataSource_OnePair(benchmark::State& state) {
    std::tuple<CountingSource> sources{CountingSource{kRounds}};
    std::tuple<CountingSink>   sinks{CountingSink{}};
    ControlChannel<1>          control;
    std::size_t                idx = control.attach();

    for (auto _ : state) {
        int rounds = kRounds;
        benchmark::DoNotOptimize(rounds);  // keep the trip count opaque, see CountingSink above
        std::get<0>(sources).remaining = rounds;
        run_data_source(sources, sinks, kLegs1, control, idx);
        benchmark::DoNotOptimize(std::get<0>(sinks).count);
    }
    state.SetItemsProcessed(state.iterations() * kRounds);
}

BENCHMARK(BM_RunDataSource_OnePair);

// Four source/sink pairs.
void BM_RunDataSource_FourPairs(benchmark::State& state) {
    std::tuple<CountingSource, CountingSource, CountingSource, CountingSource> sources{
        CountingSource{kRounds}, CountingSource{kRounds}, CountingSource{kRounds},
        CountingSource{kRounds}};
    std::tuple<CountingSink, CountingSink, CountingSink, CountingSink> sinks{};
    ControlChannel<1>                                                  control;
    std::size_t                                                        idx = control.attach();

    for (auto _ : state) {
        int rounds = kRounds;
        benchmark::DoNotOptimize(rounds);  // keep the trip count opaque, see CountingSink above
        std::get<0>(sources).remaining = rounds;
        std::get<1>(sources).remaining = rounds;
        std::get<2>(sources).remaining = rounds;
        std::get<3>(sources).remaining = rounds;
        run_data_source(sources, sinks, kLegs4, control, idx);
        benchmark::DoNotOptimize(std::get<0>(sinks).count + std::get<1>(sinks).count +
                                 std::get<2>(sinks).count + std::get<3>(sinks).count);
    }
    state.SetItemsProcessed(state.iterations() * kRounds * 4);
}

BENCHMARK(BM_RunDataSource_FourPairs);

}  // namespace
