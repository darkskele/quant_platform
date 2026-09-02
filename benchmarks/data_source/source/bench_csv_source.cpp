#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "bin_hist_venue.hpp"
#include "csv_source.hpp"
#include "support/scratch_dir.hpp"

using qp::data_source::source::CsvSource;
using qp::data_source::source::venue::binance::binance_historical::BinHistVenue;
using qp::test::ScratchDir;

namespace {

// Real tagged shape.
std::string kline_line(std::int64_t open_time_ms) {
    return "BTCUSDT,K," + std::to_string(open_time_ms) +
           ",67577.90,67680.70,67572.00,67680.40,464.748," +
           std::to_string(open_time_ms + 299'999) + ",31428593.42840,6933,289.996,19611103.65640,0";
}

// Writes `count` lines split across `num_files` files (roughly even),
// timestamps 5 minutes apart starting at `start_ms`.
std::vector<std::filesystem::path> write_kline_stream(const std::filesystem::path& dir,
                                                      std::size_t count, std::size_t num_files,
                                                      std::int64_t start_ms) {
    std::filesystem::create_directories(dir);
    std::vector<std::filesystem::path> files;
    std::size_t                        per_file = (count + num_files - 1) / num_files;
    std::size_t                        written  = 0;
    for (std::size_t f = 0; f < num_files && written < count; ++f) {
        auto          path = dir / ("file" + std::to_string(f) + ".csv");
        std::ofstream out(path);
        for (std::size_t i = 0; i < per_file && written < count; ++i, ++written) {
            out << kline_line(start_ms + static_cast<std::int64_t>(written) * 300'000) << "\n";
        }
        files.push_back(path);
    }
    return files;
}

template <class Source>
void drain(Source& source) {
    using qp::data_source::source::SourceStatus;
    for (;;) {
        auto r = source.next();
        if (r) {
            benchmark::DoNotOptimize(r);
        } else if (r.error() == SourceStatus::Eof) {
            break;
        }
    }
}

// One stream, no merge.
void BM_CsvSource_SingleStreamKlines(benchmark::State& state) {
    static constexpr std::size_t kEvents = 20'000;
    static constexpr std::size_t kFiles  = 20;

    ScratchDir dir;
    auto       files = write_kline_stream(dir.path, kEvents, kFiles, 1'717'200'000'000);

    for (auto _ : state) {
        CsvSource<BinHistVenue, 1> source({files});
        drain(source);
    }
    state.SetItemsProcessed(state.iterations() * kEvents);
}

BENCHMARK(BM_CsvSource_SingleStreamKlines);

// Same total event count as the single-stream case, spread across 4
// streams with interleaved timestamps.
void BM_CsvSource_MultiStreamMerge(benchmark::State& state) {
    static constexpr std::size_t kStreams         = 4;
    static constexpr std::size_t kEventsPerStream = 5'000;
    static constexpr std::size_t kTotalEvents     = kStreams * kEventsPerStream;

    ScratchDir                                               dir;
    std::array<std::vector<std::filesystem::path>, kStreams> streams;
    for (std::size_t s = 0; s < kStreams; ++s) {
        // Offset each stream's start by less than one bar interval so
        // next() can't just drain one stream dry before touching anothers.
        streams[s] = write_kline_stream(dir.path / ("s" + std::to_string(s)), kEventsPerStream, 4,
                                        1'717'200'000'000 + static_cast<std::int64_t>(s) * 60'000);
    }

    for (auto _ : state) {
        CsvSource<BinHistVenue, kStreams> source(streams);
        drain(source);
    }
    state.SetItemsProcessed(state.iterations() * kTotalEvents);
}

BENCHMARK(BM_CsvSource_MultiStreamMerge);

}  // namespace
