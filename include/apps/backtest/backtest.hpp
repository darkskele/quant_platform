#pragma once
#include <filesystem>
#include <optional>
#include <string>

#include "basic_risk_gate.hpp"
#include "funding_carry_strategy.hpp"
#include "partition.hpp"
#include "types.hpp"

namespace qp::backtest {

/// Root layout matches the collector's own D41 convention:
/// data_dir/futures/, data_dir/spot/, each independently written by
/// FileRecorder (own symbols.manifest). `carry`'s symbol/spot_venue/
/// futures_venue are filled in by run() (D48) — only the runtime-swept
/// threshold/size fields are meant to be set here.
struct Config {
    std::filesystem::path     data_dir;
    std::string               symbol;
    wire::DayKey              first_day{};
    wire::DayKey              last_day{};
    strategy::carry::Config   carry{};
    risk::BasicRiskGateConfig risk{};
};

/// @return nullopt on bad/missing args (usage already printed to stderr) —
///     caller should exit non-zero in that case.
std::optional<Config> parse_args(int argc, char** argv);

/// The figures actually meaningful to report once a run finishes — not
/// the raw Portfolio (an internal, flat-array-heavy representation not
/// meant for external consumption; equity()/cash()/position() are its real
/// API). A real analytics layer (Sharpe, drawdown curve, ...) is later,
/// deliberately deferred work — this is just enough to prove the wiring
/// and give a test something to assert against.
struct Results {
    Notional final_cash{};
    Notional final_equity{};
    Qty      final_spot_position{};
    Qty      final_futures_position{};
};

/// Replays data_dir/futures + data_dir/spot (filtered to config.symbol on
/// both legs) through Engine<CombinedTransport<FileReplaySource,
/// FileReplaySource>, SimClock, SimExecution<LastTradeMatcher>,
/// BasicRiskGate, 1, FundingCarryStrategy> to completion.
/// @throws std::runtime_error on a missing/mismatched symbols.manifest
///     (D48) — a config problem, not a normal "no data" case.
Results run(const Config& config);

}  // namespace qp::backtest
