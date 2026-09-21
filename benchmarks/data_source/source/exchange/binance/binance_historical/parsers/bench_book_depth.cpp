#include <benchmark/benchmark.h>

#include <string_view>

#include "book_depth.hpp"
#include "types.hpp"

namespace binance = qp::data_source::source::exchange::binance;

using binance::parsers::BookDepthParser;
using binance::parsers::parse_book_depth_group;
using qp::BookDepthBands;
using qp::BookDepthEvent;
using qp::Timestamp;

namespace {

// A real sample, BTCUSDT on 2025-06-02 at 00:00:10 UTC, as the one block the
// stream hands over.
constexpr std::string_view kSample =
    "2025-06-02 00:00:10,-5,7708.55000000,795335825.75690000\n"
    "2025-06-02 00:00:10,-4,6826.68800000,706388757.23630000\n"
    "2025-06-02 00:00:10,-3,5082.39800000,528538384.14710000\n"
    "2025-06-02 00:00:10,-2,3236.90700000,338777356.57760000\n"
    "2025-06-02 00:00:10,-1,1846.19200000,194035262.14000000\n"
    "2025-06-02 00:00:10,1,2747.22900000,291449666.44800000\n"
    "2025-06-02 00:00:10,2,4318.91700000,459767333.40140000\n"
    "2025-06-02 00:00:10,3,4985.88500000,531882812.33460000\n"
    "2025-06-02 00:00:10,4,6152.93400000,659291989.76400000\n"
    "2025-06-02 00:00:10,5,6701.42700000,719804265.48500000";

// The ten row parse alone, without the allocation that publishes it.
void BM_BinanceParser_BookDepthGroup(benchmark::State& state) {
    Timestamp      ts{};
    BookDepthBands bands{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(parse_book_depth_group(kSample, ts, bands));
        benchmark::DoNotOptimize(bands);
    }
    state.SetItemsProcessed(state.iterations());
}

// Parse plus the shared_ptr the event carries, so the allocation cost is read
// against the parse rather than guessed.
void BM_BinanceParser_BookDepthEvent(benchmark::State& state) {
    const auto& entry =
        binance::endpoint(binance::BinanceMarket::UsdM, binance::EndpointKind::BookDepth);
    Timestamp      ts{};
    BookDepthEvent event{};
    for (auto _ : state) {
        benchmark::DoNotOptimize(BookDepthParser::parse(entry, kSample, ts, event));
        benchmark::DoNotOptimize(event);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BinanceParser_BookDepthGroup);
BENCHMARK(BM_BinanceParser_BookDepthEvent);

}  // namespace
