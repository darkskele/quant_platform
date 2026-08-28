#include <benchmark/benchmark.h>

#include <vector>

#include "wire.hpp"
#include "zstd_stream.hpp"

using namespace qp;
using qp::wire::ZstdCompressor;
using qp::wire::ZstdDecompressor;

namespace {

// Same shape as bench_wire.cpp's "real book diff" — 16 bids, 8 asks, the
// dominant record type in an actual recording.
MarketEvent make_real_book_diff() {
    MarketEvent ev;
    ev.kind   = EventKind::BookDiff;
    ev.symbol = 1;
    ev.bids.resize(16);
    ev.asks.resize(8);
    for (std::size_t i = 0; i < 16; ++i) ev.bids[i] = {1000.0 + static_cast<double>(i), 1.0};
    for (std::size_t i = 0; i < 8; ++i) ev.asks[i] = {2000.0 + static_cast<double>(i), 1.0};
    return ev;
}

// Isolation: compress() alone, steady state — the per-event cost
// FileRecorder::handle_event pays. ZSTD_e_continue doesn't guarantee every
// call flushes bytes out (zstd may buffer internally) — that's real
// streaming-compressor behavior, not something to normalize away, but it
// does mean `out` grows across iterations without ever being finish()'d;
// clear it periodically (untimed) so a long run doesn't just measure
// vector reallocation.
void BM_ZstdCompressor_Compress(benchmark::State& state) {
    ZstdCompressor         compressor;
    std::vector<std::byte> encoded;
    wire::write_event(encoded, make_real_book_diff());

    std::vector<std::byte> out;
    int                    i = 0;
    for (auto _ : state) {
        if (i % 1000 == 0 && i != 0) {
            state.PauseTiming();
            out.clear();
            state.ResumeTiming();
        }
        compressor.compress(encoded, out);
        ++i;
    }
}

BENCHMARK(BM_ZstdCompressor_Compress);

// Isolation: finish() alone — ends the frame (ZSTD_e_end), only called on
// clean rotation/close, not per event. A fresh compressor each call: once
// a frame ends, that ZSTD_CCtx has said everything it's going to for this
// frame — matches FileRecorder opening a fresh ZstdCompressor per segment.
void BM_ZstdCompressor_Finish(benchmark::State& state) {
    std::vector<std::byte> encoded;
    wire::write_event(encoded, make_real_book_diff());

    std::vector<std::byte> out;
    for (auto _ : state) {
        ZstdCompressor compressor;
        compressor.compress(encoded, out);
        compressor.finish(out);
        out.clear();
    }
}

BENCHMARK(BM_ZstdCompressor_Finish);

// Isolation: decompress() alone. A fresh ZstdDecompressor per call, same
// reasoning as BM_ZstdCompressor_Finish — "a single instance decodes
// exactly one zstd frame's worth of history... a fresh instance per
// segment file" (zstd_stream.hpp) — decoding the same already-finished
// frame repeatedly needs a fresh decoder each time, not a reused one fed
// the same bytes twice (that isn't a valid stream continuation).
void BM_ZstdDecompressor_Decompress(benchmark::State& state) {
    std::vector<std::byte> encoded;
    wire::write_event(encoded, make_real_book_diff());
    std::vector<std::byte> compressed;
    {
        ZstdCompressor compressor;
        compressor.compress(encoded, compressed);
        compressor.finish(compressed);
    }

    std::vector<std::byte> out;
    for (auto _ : state) {
        ZstdDecompressor decompressor;
        decompressor.decompress(compressed, out);
        benchmark::DoNotOptimize(out.data());
        out.clear();
    }
}

BENCHMARK(BM_ZstdDecompressor_Decompress);

// Tandem: full compress-finish-decompress round trip, one event — the
// real end-to-end cost of moving one event through the zstd layer, same
// shape as bench_wire.cpp's own round-trip benchmarks for the wire codec
// layer above this one.
void BM_ZstdStream_RoundTrip(benchmark::State& state) {
    std::vector<std::byte> encoded;
    wire::write_event(encoded, make_real_book_diff());

    std::vector<std::byte> compressed;
    std::vector<std::byte> decompressed;
    for (auto _ : state) {
        compressed.clear();
        {
            ZstdCompressor compressor;
            compressor.compress(encoded, compressed);
            compressor.finish(compressed);
        }
        decompressed.clear();
        ZstdDecompressor decompressor;
        decompressor.decompress(compressed, decompressed);
        benchmark::DoNotOptimize(decompressed.data());
    }
}

BENCHMARK(BM_ZstdStream_RoundTrip);

}  // namespace
