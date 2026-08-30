#include <benchmark/benchmark.h>

#include "wire.hpp"

using namespace qp;
using namespace qp::data_source::wire;

namespace {

MarketEvent make_book_diff(std::size_t bid_count, std::size_t ask_count) {
    BookDiffEvent ev;
    ev.ts        = 1786742884159;
    ev.first_seq = 11289273841677;
    ev.seq       = 11289273848795;
    ev.prev_seq  = 11289273841549;
    ev.symbol    = 1;
    auto levels  = std::make_shared<BookLevels>();
    levels->bids.resize(bid_count);
    levels->asks.resize(ask_count);
    for (std::size_t i = 0; i < bid_count; ++i)
        levels->bids[i] = {1000.0 + static_cast<double>(i), 1.0};
    for (std::size_t i = 0; i < ask_count; ++i)
        levels->asks[i] = {2000.0 + static_cast<double>(i), 1.0};
    ev.levels = std::move(levels);
    return ev;
}

// A BookSnapshot at Binance's actual max depth (a REST depth-snapshot
// fetch's own hardcoded limit=1000) — the realistic upper bound this
// format has to handle. Bigger than any BookDiff bench here by ~60x (1000
// vs 16 levels) — worth its own number, not assumed to scale linearly from
// the small case.
MarketEvent make_book_snapshot(std::size_t levels) {
    // Materialize into a named local first, not
    // std::get<BookDiffEvent>(make_book_diff(...)) bound directly to a
    // reference: std::get<T>(variant&&) returns T&&, a reference into the
    // temporary variant make_book_diff() returned — binding that to
    // `const auto&` does NOT extend the temporary's lifetime (lifetime
    // extension doesn't propagate through a function call boundary), so
    // the temporary is destroyed at the end of that statement and `diff`
    // dangles for the rest of the function. Real bug, caught by a real
    // SIGSEGV in qp_bench, not theoretical.
    MarketEvent       diff_event = make_book_diff(levels, levels);
    const auto&       diff       = std::get<BookDiffEvent>(diff_event);
    BookSnapshotEvent ev;
    ev.ts     = diff.ts;
    ev.symbol = diff.symbol;
    ev.levels = diff.levels;
    return ev;
}

MarketEvent make_trade() {
    TradeEvent ev;
    ev.ts     = 1786742457242;
    ev.symbol = 1;
    ev.price  = 62859.0;
    ev.qty    = 0.938;
    ev.side   = Side::Buy;
    return ev;
}

// `buf` is reused (cleared, not reallocated) across iterations, same
// reasoning as the parser bench reusing `ev`: after the first iteration its
// capacity covers the record, so this measures steady-state append cost,
// not allocator noise.

void BM_Wire_WriteSmallBookDiff(benchmark::State& state) {
    auto                   ev = make_book_diff(2, 1);  // a couple of levels, quiet-book update
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_WriteSmallBookDiff);

void BM_Wire_WriteRealBookDiff(benchmark::State& state) {
    auto ev = make_book_diff(16, 8);  // same shape as the parser bench's real capture
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_WriteRealBookDiff);

void BM_Wire_WriteTrade(benchmark::State& state) {
    auto                   ev = make_trade();
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_WriteTrade);

void BM_Wire_ReadRealBookDiff(benchmark::State& state) {
    auto                   ev = make_book_diff(16, 8);
    std::vector<std::byte> buf;
    write_event(buf, ev);
    for (auto _ : state) {
        std::span<const std::byte> cursor{buf};
        auto                       decoded = read_event(cursor);
        benchmark::DoNotOptimize(decoded);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_ReadRealBookDiff);

// End-to-end cost of what FileRecorder/FileReplaySource actually do per
// event: encode, then (on the read side, later) decode. Not the same as
// write+read summed in isolation once compression enters the picture, but
// today (pre-zstd) it's the real number for "cost of moving one event
// through the format."
void BM_Wire_RoundTripRealBookDiff(benchmark::State& state) {
    auto                   ev = make_book_diff(16, 8);
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        std::span<const std::byte> cursor{buf};
        auto                       decoded = read_event(cursor);
        benchmark::DoNotOptimize(decoded);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_RoundTripRealBookDiff);

void BM_Wire_WriteFullDepthSnapshot(benchmark::State& state) {
    auto                   ev = make_book_snapshot(1000);
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_WriteFullDepthSnapshot);

void BM_Wire_ReadFullDepthSnapshot(benchmark::State& state) {
    auto                   ev = make_book_snapshot(1000);
    std::vector<std::byte> buf;
    write_event(buf, ev);
    for (auto _ : state) {
        std::span<const std::byte> cursor{buf};
        auto                       decoded = read_event(cursor);
        benchmark::DoNotOptimize(decoded);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_ReadFullDepthSnapshot);

void BM_Wire_RoundTripFullDepthSnapshot(benchmark::State& state) {
    auto                   ev = make_book_snapshot(1000);
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        std::span<const std::byte> cursor{buf};
        auto                       decoded = read_event(cursor);
        benchmark::DoNotOptimize(decoded);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Wire_RoundTripFullDepthSnapshot);

}  // namespace
