#include <benchmark/benchmark.h>

#include <string_view>

#include "mark_klines.hpp"
#include "premium_klines.hpp"
#include "types.hpp"

using qp::MarkPriceKlineEvent;
using qp::PremiumIndexKlineEvent;
using qp::Timestamp;
using qp::data_source::source::venue::binance::parsers::parse_mark_klines_row;
using qp::data_source::source::venue::binance::parsers::parse_premium_klines_row;

namespace {

constexpr std::string_view kMarkRow =
    "1748822400000,105589.61297464,105683.60000000,105362.40000000,105378.90000000,0,1748825999999,"
    "0,3600,0,0,0";

// Short negative rates rather than long prices, so the digit scan does less work.
constexpr std::string_view kPremiumRow =
    "1748822400000,-0.00057025,0,-0.00080144,-0.00049096,0,1748825999999,0,720,0,0,0";

void BM_ParseMarkKlineRow(benchmark::State& state) {
    Timestamp           ts{};
    MarkPriceKlineEvent mark{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_mark_klines_row(kMarkRow, ts, mark));
        benchmark::DoNotOptimize(mark);
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_ParsePremiumKlineRow(benchmark::State& state) {
    Timestamp              ts{};
    PremiumIndexKlineEvent premium{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_premium_klines_row(kPremiumRow, ts, premium));
        benchmark::DoNotOptimize(premium);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ParseMarkKlineRow);
BENCHMARK(BM_ParsePremiumKlineRow);

}  // namespace
