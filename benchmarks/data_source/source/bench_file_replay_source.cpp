#include <benchmark/benchmark.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "file_replay_source.hpp"
#include "partition.hpp"
#include "support/market_event_builders.hpp"
#include "support/scratch_dir.hpp"
#include "wire.hpp"
#include "zstd_stream.hpp"

using namespace qp;
using qp::source::FileReplaySource;
using qp::test::ScratchDir;
using qp::wire::DayKey;

namespace {

// Same shape as the real capture in bench_message_processing.cpp / the wire
// bench's "real book diff" case (16 bids, 8 asks) — the dominant record
// type in an actual recording, not a toy.
MarketEvent book_diff(SymbolId symbol, std::int64_t ts, std::uint64_t seq) {
    std::vector<PriceLevel> bids(16), asks(8);
    for (std::size_t i = 0; i < bids.size(); ++i) bids[i] = {1000.0 + double(i), 1.0};
    for (std::size_t i = 0; i < asks.size(); ++i) asks[i] = {2000.0 + double(i), 1.0};
    return qp::test::make_book_diff(symbol, ts, seq, seq + 7, seq - 1, std::move(bids),
                                    std::move(asks));
}

MarketEvent trade(SymbolId symbol, std::int64_t ts) {
    return qp::test::make_trade(symbol, ts, 62859.0, 0.938, Side::Buy);
}

// Writes one on-disk segment exactly as FileRecorder does: every event
// pushed through one ZstdCompressor, one finish()'d frame per file. Hand-
// rolled from qp_wire's own primitives rather than driving a real
// FileRecorder — fine here (unlike tests/test_file_replay_source.cpp,
// which must use the real object): this benchmark only needs realistic-
// shaped input to time FileReplaySource's read path against, not a
// write/read agreement proof — that's the test file's and
// tests/test_recorder_replay_parity.cpp's job.
void write_segment(const std::filesystem::path& path, const std::vector<MarketEvent>& events) {
    std::filesystem::create_directories(path.parent_path());

    wire::ZstdCompressor   compressor;
    std::vector<std::byte> compressed;
    for (const auto& ev : events) {
        std::vector<std::byte> encoded;
        wire::write_event(encoded, ev);
        compressor.compress(encoded, compressed);
    }
    compressor.finish(compressed);

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(compressed.data()),
              static_cast<std::streamsize>(compressed.size()));
}

// FileReplaySource now requires data_dir/symbols.manifest (see
// FileRecorder::write_symbols_manifest) — hand-written here for the same
// reason write_segment is: this benchmark bypasses FileRecorder on
// purpose, it just needs the fixture to look like real recorder output.
void write_symbols_manifest(const std::filesystem::path&    data_dir,
                            const std::vector<std::string>& symbol_names) {
    std::ofstream out(data_dir / "symbols.manifest");
    for (const auto& name : symbol_names) out << name << '\n';
}

constexpr DayKey kDay = 19723;  // 2024-01-01, arbitrary — only ever compared to itself

// Drains a freshly constructed FileReplaySource fully — the realistic unit
// of work: replay isn't "call next() once," it's "read a whole
// day/symbol-range." Amortizes construction (segment listing, first
// decompress) across the run same as it would in an actual backtest.
void drain(FileReplaySource& source) {
    while (auto ev = source.next()) benchmark::DoNotOptimize(ev);
}

}  // namespace

// One symbol, BookDiff-heavy — the realistic case: this record type
// dominates an actual recording (bids/asks dwarf the fixed portion, see
// wire.hpp), so it's what replay throughput actually lives or dies on.
void BM_FileReplaySource_SingleSymbolBookDiffs(benchmark::State& state) {
    static constexpr std::size_t kEvents = 20'000;

    ScratchDir dir;
    auto       symbol_dir = dir.path / "BTCUSDT";

    std::vector<MarketEvent> events;
    events.reserve(kEvents);
    for (std::size_t i = 0; i < kEvents; ++i) {
        events.push_back(book_diff(0, static_cast<std::int64_t>(i), 100 + i * 7));
    }
    write_segment(wire::segment_path(symbol_dir, kDay, 0), events);
    write_symbols_manifest(dir.path, {"BTCUSDT"});

    for (auto _ : state) {
        FileReplaySource source(dir.path, kDay, kDay);
        drain(source);
    }
    state.SetItemsProcessed(state.iterations() * kEvents);
}

BENCHMARK(BM_FileReplaySource_SingleSymbolBookDiffs);

// One symbol, Trade-only — cheap, fixed-size records (no bid/ask levels):
// isolates per-event/per-call overhead (decompress-loop, wire::read_event,
// the k-way-merge scan) from the level-copying cost BookDiff adds on top.
void BM_FileReplaySource_SingleSymbolTrades(benchmark::State& state) {
    static constexpr std::size_t kEvents = 50'000;

    ScratchDir dir;
    auto       symbol_dir = dir.path / "BTCUSDT";

    std::vector<MarketEvent> events;
    events.reserve(kEvents);
    for (std::size_t i = 0; i < kEvents; ++i)
        events.push_back(trade(0, static_cast<std::int64_t>(i)));
    write_segment(wire::segment_path(symbol_dir, kDay, 0), events);
    write_symbols_manifest(dir.path, {"BTCUSDT"});

    for (auto _ : state) {
        FileReplaySource source(dir.path, kDay, kDay);
        drain(source);
    }
    state.SetItemsProcessed(state.iterations() * kEvents);
}

BENCHMARK(BM_FileReplaySource_SingleSymbolTrades);

// Same total event count as the single-symbol trade case, spread across 4
// symbols with interleaved timestamps — isolates the k-way merge's
// per-event linear scan (next()'s cost of picking the earliest of N
// cursors) from the single-cursor case above. Difference between this and
// BM_FileReplaySource_SingleSymbolTrades' per-item cost is roughly the merge overhead.
void BM_FileReplaySource_MultiSymbolMerge(benchmark::State& state) {
    static constexpr std::size_t kSymbols         = 4;
    static constexpr std::size_t kEventsPerSymbol = 12'500;
    static constexpr std::size_t kTotalEvents     = kSymbols * kEventsPerSymbol;

    ScratchDir               dir;
    std::vector<std::string> symbol_names = {"S0", "S1", "S2", "S3"};

    for (SymbolId sym = 0; sym < kSymbols; ++sym) {
        std::vector<MarketEvent> events;
        events.reserve(kEventsPerSymbol);
        for (std::size_t i = 0; i < kEventsPerSymbol; ++i) {
            // Timestamps interleave across symbols (sym is the fastest-
            // varying component) so next() can't just drain one cursor
            // dry before touching another — every call has to pick a
            // winner among all 4.
            std::int64_t ts = static_cast<std::int64_t>(i * kSymbols + sym);
            events.push_back(trade(sym, ts));
        }
        write_segment(wire::segment_path(dir.path / symbol_names[sym], kDay, 0), events);
    }
    write_symbols_manifest(dir.path, symbol_names);

    for (auto _ : state) {
        FileReplaySource source(dir.path, kDay, kDay);
        drain(source);
    }
    state.SetItemsProcessed(state.iterations() * kTotalEvents);
}

BENCHMARK(BM_FileReplaySource_MultiSymbolMerge);
