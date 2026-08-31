#pragma once
#include <cstddef>
#include <filesystem>
#include <string_view>

#include "backtest_in_process_transport.hpp"
#include "data_layout.hpp"
#include "engine.hpp"
#include "engine_config.hpp"
#include "futures_leg.hpp"
#include "sim_clock.hpp"
#include "sim_execution.hpp"
#include "sink_config.hpp"
#include "spot_leg.hpp"
#include "venues.hpp"

#ifndef QP_BACKTEST_DATA_DIR
#define QP_BACKTEST_DATA_DIR "data/binance_historical"
#endif
#ifndef QP_BACKTEST_SYMBOL
#define QP_BACKTEST_SYMBOL "BTCUSDT"
#endif
#ifndef QP_BACKTEST_FIRST_DAY
#define QP_BACKTEST_FIRST_DAY "2024-01-01"
#endif
#ifndef QP_BACKTEST_LAST_DAY
#define QP_BACKTEST_LAST_DAY "2024-01-01"
#endif

// The one place backtest.cpp/backtest.hpp look for a concrete type or
// value. Every macro-selected combo (source_config.hpp/sink_config.hpp/
// engine_config.hpp), the venue/symbol universe (venues.hpp), and which
// data this binary reads (QP_BACKTEST_* above, baked in by
// tools/build_backtest.sh — never argv, see D5x) get assembled here, once.
namespace qp::backtest::config {

inline constexpr std::size_t kNumVenues = std::tuple_size_v<VenueTables>;

// Not tuned against real replay throughput yet — same "placeholder, not
// validated" status as SpmcRing's own backoff constants until this path
// gets benchmarked for real.
inline constexpr std::size_t kRingCapacity = 1024;

// This backtest is Engine's sole consumer of each leg's ring.
using Sink = LegSink<kRingCapacity, /*NumConsumers=*/1>;

using Matcher  = MatcherType<Book>;
using Exec     = execution::sim::SimExecution<Matcher, Book>;
using Risk     = RiskType<Book>;
using Strategy = StrategyType<Book>;
using Tx       = engine::transport::BacktestInProcessTransport<Sink::Ring, kNumVenues,
                                                               /*NumControlConsumers=*/1>;

using EngineType = engine::Engine<Tx, SimClock, Exec, Risk, Strategy, Book>;

// What this binary trades — decided at build time (tools/build_backtest.sh
// / QP_BACKTEST_* defines), not by a runtime flag: matches D5x's "no
// runtime params" call, and it's what a future pybind11 wrapper wants
// anyway — the compiled extension module IS one fixed (data, components)
// choice; only strategy/risk knobs (Config::carry/risk, backtest.hpp) stay
// runtime-settable, since those are exactly what a Python-side optimizer
// sweeps run over run.
inline constexpr std::string_view kDataDirStr = QP_BACKTEST_DATA_DIR;
inline constexpr std::string_view kSymbol     = QP_BACKTEST_SYMBOL;

static_assert(FuturesTable::id_of(kSymbol).has_value(),
              "QP_BACKTEST_SYMBOL isn't in config::FuturesTable's compile-time symbol universe");
inline constexpr SymbolId kSymbolId = *FuturesTable::id_of(kSymbol);

static_assert(parse_day(QP_BACKTEST_FIRST_DAY).has_value(), "QP_BACKTEST_FIRST_DAY: bad date");
static_assert(parse_day(QP_BACKTEST_LAST_DAY).has_value(), "QP_BACKTEST_LAST_DAY: bad date");
inline constexpr auto kFirstDay = *parse_day(QP_BACKTEST_FIRST_DAY);
inline constexpr auto kLastDay  = *parse_day(QP_BACKTEST_LAST_DAY);

inline std::filesystem::path data_dir() { return std::filesystem::path{kDataDirStr}; }

}  // namespace qp::backtest::config
