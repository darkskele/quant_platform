#pragma once
#include <cstddef>

#include "backtest_in_process_transport.hpp"
#include "engine.hpp"
#include "engine_config.hpp"
#include "futures_leg.hpp"
#include "sim_execution.hpp"
#include "sink_config.hpp"
#include "spot_leg.hpp"
#include "venues.hpp"

// The one place the backtest variants look for a concrete composed type.
namespace qp::backtest::config {

inline constexpr std::size_t kNumVenues = std::tuple_size_v<VenueTables>;

// Not tuned against real replay throughput yet. @todo
inline constexpr std::size_t kRingCapacity = 1024;

// This backtest is Engine's sole consumer of each leg's queue.
using Sink = LegSink<kRingCapacity, /*NumConsumers=*/1>;

using Matcher  = MatcherType<Book>;
using Exec     = execution::sim::SimExecution<Matcher, Book>;
using Risk     = RiskType<Book>;
using Strategy = StrategyType<Book>;
using Tx       = engine::transport::BacktestInProcessTransport<kRingCapacity, kNumVenues>;

template <class Recorder = engine::NullRecorder>
using EngineType = engine::Engine<Tx, Exec, Risk, Strategy, Book, Recorder>;

}  // namespace qp::backtest::config
