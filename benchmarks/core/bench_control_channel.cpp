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

// Isolation: request_stop() alone. Periodically (untimed) pump()s so the
// underlying MpscQueue<ControlCommand, 8> (kCapacity=8) never actually
// fills during a timed iteration. pump() itself broadcasts each drained
// request into the SpmcRing<ControlCommand, 8, 1> side — with nobody
// polling that ring, it would fill after 8 broadcasts and block forever
// inside push()'s wait_for_slot (hit this for real: first version of this
// benchmark hung). Draining via poll(0) right after pump() avoids it.
void BM_ControlChannel_RequestStop(benchmark::State& state) {
    ControlChannel<1> control;
    int               i = 0;
    for (auto _ : state) {
        if (i % 7 == 0 && i != 0) {
            state.PauseTiming();
            control.pump();
            while (control.poll(0)) {
            }
            state.ResumeTiming();
        }
        bool ok = control.request_stop();
        benchmark::DoNotOptimize(ok);
        ++i;
    }
}

BENCHMARK(BM_ControlChannel_RequestStop);

// Isolation: poll() alone. Periodically (untimed) broadcasts so the
// underlying SpmcRing<ControlCommand, 8, 1> never runs dry during a timed
// iteration.
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

// Tandem: the real intended round trip, single thread — a participant
// requests stop, the owner's pump() drains and broadcasts it, the
// participant sees it via poll(). No real concurrency.
void BM_ControlChannel_RequestPumpPoll(benchmark::State& state) {
    ControlChannel<1> control;
    for (auto _ : state) {
        bool requested = control.request_stop();
        benchmark::DoNotOptimize(requested);
        control.pump();
        auto cmd = control.poll(0);
        benchmark::DoNotOptimize(cmd);
    }
}

BENCHMARK(BM_ControlChannel_RequestPumpPoll);

}  // namespace
