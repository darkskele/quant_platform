#include <benchmark/benchmark.h>

#include <string_view>

#include "funding.hpp"
#include "types.hpp"

using qp::FundingEvent;
using qp::Timestamp;
using qp::data_source::source::venue::binance::parsers::parse_funding_row;

namespace {

constexpr std::string_view kFundingRow = "1748736000001,8,-0.00000582";

void BM_ParseFundingRow(benchmark::State& state) {
    Timestamp    ts{};
    FundingEvent funding{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_funding_row(kFundingRow, ts, funding));
        benchmark::DoNotOptimize(funding);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_ParseFundingRow);

}  // namespace
