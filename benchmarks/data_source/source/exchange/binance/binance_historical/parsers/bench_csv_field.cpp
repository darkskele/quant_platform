#include <benchmark/benchmark.h>

#include <string_view>

#include "csv_field.hpp"
#include "types.hpp"

using qp::Timestamp;
using qp::data_source::source::exchange::binance::parsers::take_datetime;
using qp::data_source::source::exchange::binance::parsers::take_stamp;

namespace {

// The two stamp formats the bucket publishes, as they appear at the head of a
// row. Paired here so the fixed width path is read against the integer path it
// sits beside rather than in isolation.
constexpr std::string_view kEpochRow    = "1748822400000,0.5";
constexpr std::string_view kDatetimeRow = "2025-06-02 00:00:10,-5";

void BM_BinanceCsvField_TakeStamp(benchmark::State& state) {
    Timestamp out{};
    for (auto _ : state) {
        std::string_view row = kEpochRow;
        benchmark::DoNotOptimize(take_stamp(row, out));
        benchmark::DoNotOptimize(out);
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_BinanceCsvField_TakeDatetime(benchmark::State& state) {
    Timestamp out{};
    for (auto _ : state) {
        std::string_view row = kDatetimeRow;
        benchmark::DoNotOptimize(take_datetime(row, out));
        benchmark::DoNotOptimize(out);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinanceCsvField_TakeStamp);
BENCHMARK(BM_BinanceCsvField_TakeDatetime);

}  // namespace
