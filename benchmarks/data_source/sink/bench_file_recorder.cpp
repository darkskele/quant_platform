#include <benchmark/benchmark.h>

#include <vector>

#include "file_recorder.hpp"
#include "support/scratch_dir.hpp"
#include "types.hpp"

using qp::sink::FileRecorder;
using qp::test::ScratchDir;

namespace {

qp::MarketEvent make_trade() {
    qp::MarketEvent ev;
    ev.kind   = qp::EventKind::Trade;
    ev.symbol = 0;
    ev.price  = 100.0;
    ev.qty    = 1.0;
    return ev;
}

// Isolation: record() call-side cost only. Unlike the core queues'
// contention benchmarks, this needs no manual ->Threads() setup —
// FileRecorder always owns and runs a real background writer thread
// internally (its constructor spawns it), so a plain single-threaded call
// to record() is already measuring the caller side of genuine
// producer/consumer concurrency, not an artificially uncontended case.
// Non-blocking by design (drops silently on a full queue, see
// dropped_count()) — no periodic draining needed the way the core queues'
// isolation benches need it. The reported time is the caller-side cost of
// one record() call, NOT a sustainable recording rate: calling it back-
// to-back with no gap outpaces the real writer thread's disk+zstd
// throughput, so most calls measured here actually get dropped (~70% in a
// typical run — see the "dropped" counter) rather than fully processed.
// That's expected and correct for what this isolates (the caller's own
// cost), just not the number to quote for "how many events/sec can this
// actually record."
void BM_FileRecorder_Record(benchmark::State& state) {
    ScratchDir   dir;
    FileRecorder recorder(dir.path, std::vector<std::string>{"BTCUSDT"});
    for (auto _ : state) recorder.record(make_trade());
    state.counters["dropped"] = static_cast<double>(recorder.dropped_count());
}

BENCHMARK(BM_FileRecorder_Record);

// Public-API-only throughput: construct, push kEvents, then let the
// destructor run *inside* the timed region — its drain loop
// (writer_run()'s post-loop "clean shutdown: drain whatever's left, then
// close every partition properly") blocks until every pushed event is
// actually encoded, compressed, and written, not just enqueued. That's
// the real sustained-throughput number BM_FileRecorder_Record's caller-
// side-only cost can't give you, using nothing but record() and the
// destructor — no access to handle_event() needed.
//
// kEvents stays comfortably under kQueueCapacity (2048, file_recorder.hpp)
// so nothing drops before the writer even gets a chance to drain — the
// "dropped" counter below should read 0 every run; if it doesn't, kEvents
// is too close to capacity for how fast this box's writer thread drains.
void BM_FileRecorder_RecordAndDrain(benchmark::State& state) {
    constexpr int kEvents = 1000;
    ScratchDir    dir;
    std::size_t   total_dropped = 0;
    for (auto _ : state) {
        FileRecorder recorder(dir.path, std::vector<std::string>{"BTCUSDT"});
        for (int i = 0; i < kEvents; ++i) recorder.record(make_trade());
        total_dropped += recorder.dropped_count();
    }  // destructor: drains the queue, flushes, and closes the partition — timed
    state.SetItemsProcessed(state.iterations() * kEvents);
    state.counters["dropped"] = static_cast<double>(total_dropped);
}

BENCHMARK(BM_FileRecorder_RecordAndDrain);

}  // namespace
