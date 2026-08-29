#include <benchmark/benchmark.h>

#include "control_channel.hpp"

using qp::ControlChannel;
using qp::ControlCommand;

namespace {

// No contention tier here, unlike SpscQueue/SpmcRing/MpscQueue: this class
// is explicitly a low-frequency control-plane primitive (control_channel.hpp:
// "control commands are rare... not a data-rate queue"), both queues sized
// for a handful of in-flight commands (kCapacity = 8), not sustained
// multi-thread throughput. Forcing a producer/consumer contention
// benchmark onto it would mean fighting the same supply/demand-imbalance
// deadlock class documented in bench_mpsc_queue.cpp for a component that's
// never actually hammered like that in production.
//
// pump() isn't benchmarked directly any more, and there's no
// request-stop-then-pump-then-poll round trip here either: pump() is
// private now, driven by ControlChannel's own background thread on a
// ~100ms cadence (control_channel.hpp), not something a caller invokes
// synchronously. That round trip is inherently asynchronous now and is
// covered as a correctness property in test_control_channel.cpp instead
// of a tight-loop timing here.

// Isolation: request_stop() alone, steady state. The request inbox
// (kCapacity=8) fills within the first few iterations of a tight timed
// loop — the background pump thread only drains it every ~100ms, far
// slower than this loop — so past the first handful of calls, this
// measures the full-inbox fast-reject path (push() reports full, no
// blocking), which is the realistic steady state for any caller hammering
// request_stop() faster than the pump interval.
//
// poll() every iteration isn't optional here, unlike the old pump()-based
// version's periodic drain: broadcast_ is gated (SpmcRing never drops,
// blocks until a consumer catches up), and with nobody polling, the pump
// thread's own broadcast() call blocks forever the moment a second pump
// cycle tries to push into an already-full, never-drained ring — wedging
// the pump thread permanently and hanging ~ControlChannel()'s join() on
// process exit (hit this for real: first version of this benchmark hung).
// Draining every iteration, not periodically, because the pump thread
// fires on its own ~100ms clock now, not synchronously with this loop —
// there's no call-count-based interval left that's guaranteed to catch it.
void BM_ControlChannel_RequestStop(benchmark::State& state) {
    ControlChannel<1> control;
    auto              consumer = control.attach();
    for (auto _ : state) {
        bool ok = control.request_stop();
        benchmark::DoNotOptimize(ok);
        control.poll(consumer);
    }
}

BENCHMARK(BM_ControlChannel_RequestStop);

// Isolation: poll() alone. Periodically (untimed) broadcasts so the
// underlying SpmcRing<ControlCommand, 8, 1> never runs dry during a timed
// iteration. Unaffected by the pump() change: broadcast() bypasses the
// request inbox entirely, same as before.
void BM_ControlChannel_Poll(benchmark::State& state) {
    ControlChannel<1> control;
    for (int j = 0; j < 7; ++j) control.broadcast(ControlCommand::Start);
    for (auto _ : state) {
        auto cmd = control.poll(0);
        if (!cmd) {
            state.PauseTiming();
            for (int j = 0; j < 7; ++j) control.broadcast(ControlCommand::Start);
            state.ResumeTiming();
            cmd = control.poll(0);
        }
        benchmark::DoNotOptimize(cmd);
    }
}

BENCHMARK(BM_ControlChannel_Poll);

}  // namespace
