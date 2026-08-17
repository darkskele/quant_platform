#include <benchmark/benchmark.h>

#include "wire.hpp"

using namespace qp;
using namespace qp::wire;

namespace {

MarketEvent make_book_diff(std::size_t bid_count, std::size_t ask_count) {
    MarketEvent ev;
    ev.kind      = EventKind::BookDiff;
    ev.ts        = 1786742884159;
    ev.first_seq = 11289273841677;
    ev.seq       = 11289273848795;
    ev.prev_seq  = 11289273841549;
    ev.symbol    = 1;
    ev.bids.resize(bid_count);
    ev.asks.resize(ask_count);
    for (std::size_t i = 0; i < bid_count; ++i) ev.bids[i] = {1000.0 + static_cast<double>(i), 1.0};
    for (std::size_t i = 0; i < ask_count; ++i) ev.asks[i] = {2000.0 + static_cast<double>(i), 1.0};
    return ev;
}

// A BookSnapshot at Binance's actual max depth (fetch_depth_snapshot's own
// hardcoded limit=1000) — the realistic upper bound now flowing through
// this exact code path since ResyncCoordinator started forwarding the
// resync snapshot as an event instead of discarding its levels. Bigger
// than any BookDiff bench here by ~60x (1000 vs 16 levels) — worth its own
// number, not assumed to scale linearly from the small case.
MarketEvent make_book_snapshot(std::size_t levels) {
    MarketEvent ev = make_book_diff(levels, levels);
    ev.kind        = EventKind::BookSnapshot;
    return ev;
}

MarketEvent make_trade() {
    MarketEvent ev;
    ev.kind   = EventKind::Trade;
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

void BM_WriteEventSmallBookDiff(benchmark::State& state) {
    auto                   ev = make_book_diff(2, 1);  // a couple of levels, quiet-book update
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_WriteEventSmallBookDiff);

void BM_WriteEventRealBookDiff(benchmark::State& state) {
    auto ev = make_book_diff(16, 8);  // same shape as the parser bench's real capture
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_WriteEventRealBookDiff);

void BM_WriteEventTrade(benchmark::State& state) {
    auto                   ev = make_trade();
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_WriteEventTrade);

void BM_ReadEventRealBookDiff(benchmark::State& state) {
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

BENCHMARK(BM_ReadEventRealBookDiff);

// End-to-end cost of what FileRecorder/FileReplaySource actually do per
// event: encode, then (on the read side, later) decode. Not the same as
// write+read summed in isolation once compression enters the picture, but
// today (pre-zstd) it's the real number for "cost of moving one event
// through the format."
void BM_RoundTripRealBookDiff(benchmark::State& state) {
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

BENCHMARK(BM_RoundTripRealBookDiff);

void BM_WriteEventFullDepthSnapshot(benchmark::State& state) {
    auto                   ev = make_book_snapshot(1000);
    std::vector<std::byte> buf;
    for (auto _ : state) {
        buf.clear();
        write_event(buf, ev);
        benchmark::DoNotOptimize(buf.data());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_WriteEventFullDepthSnapshot);

void BM_ReadEventFullDepthSnapshot(benchmark::State& state) {
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

BENCHMARK(BM_ReadEventFullDepthSnapshot);

void BM_RoundTripFullDepthSnapshot(benchmark::State& state) {
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

BENCHMARK(BM_RoundTripFullDepthSnapshot);

}  // namespace
