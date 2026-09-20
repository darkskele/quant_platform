#include <benchmark/benchmark.h>

#include <string>
#include <string_view>
#include <vector>

#include "endpoints.hpp"
#include "klines.hpp"
#include "types.hpp"

using qp::KlineEvent;
using qp::Timestamp;
using qp::data_source::source::venue::binance::parsers::parse_klines_row;

using namespace qp::data_source::source::venue::binance;

namespace {

const auto& kUsdM = endpoint(BinanceMarket::UsdM, EndpointKind::Klines);

constexpr std::string_view kUsdMRowMillis =
    "1748822400000,105583.30,105700.00,105351.80,105379.10,3928.600,1748825999999,414423568.46600,"
    "97149,1862.868,196511690.26110,0";

constexpr std::string_view kSpotRowMicros =
    "1748822400000000,105642.93000000,105735.42000000,105404.31000000,105427.48000000,432.43804000,"
    "1748825999999999,45644388.03464300,124446,192.02670000,20268734.77003200,0";

// One day of one minute bars, the smallest partition the fetch loop handles.
constexpr std::size_t kRowsPerDay = 1440;

std::vector<std::string> make_day() {
    std::vector<std::string> rows;
    rows.reserve(kRowsPerDay);
    for (std::size_t i = 0; i < kRowsPerDay; ++i) {
        std::string row;
        row += std::to_string(1748822400000LL + static_cast<long long>(i) * 60'000);
        row += kUsdMRowMillis.substr(kUsdMRowMillis.find(','));
        rows.push_back(std::move(row));
    }
    return rows;
}

void BM_BinanceParser_KlineRowMillis(benchmark::State& state) {
    Timestamp  ts{};
    KlineEvent kline{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_klines_row(kUsdM, kUsdMRowMillis, ts, kline));
        benchmark::DoNotOptimize(kline);
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_BinanceParser_KlineRowMicros(benchmark::State& state) {
    Timestamp  ts{};
    KlineEvent kline{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_klines_row(kUsdM, kSpotRowMicros, ts, kline));
        benchmark::DoNotOptimize(kline);
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_BinanceParser_KlineDay(benchmark::State& state) {
    const auto rows = make_day();
    Timestamp  ts{};
    KlineEvent kline{};
    for (auto _ : state) {
        for (const auto& row : rows) {
            benchmark::DoNotOptimize(parse_klines_row(kUsdM, row, ts, kline));
            benchmark::DoNotOptimize(kline);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(rows.size()));
}

BENCHMARK(BM_BinanceParser_KlineRowMillis);
BENCHMARK(BM_BinanceParser_KlineRowMicros);
BENCHMARK(BM_BinanceParser_KlineDay);

}  // namespace
