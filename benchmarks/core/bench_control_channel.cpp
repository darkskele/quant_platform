#include <benchmark/benchmark.h>

#include "control_channel.hpp"

using qp::ControlChannel;
using qp::ControlCommand;

namespace {

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
