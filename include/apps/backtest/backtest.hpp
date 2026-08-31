#pragma once
#include <optional>

#include "basic_risk_gate.hpp"
#include "funding_carry_strategy.hpp"
#include "types.hpp"

namespace qp::backtest {

/// Deliberately just the runtime-swept knobs — which symbol, which data,
/// and which Source/Sink/Matcher/RiskGate/Strategy types are all decided
/// at build time (config::kSymbol/kDataDir/kFirstDay/kLastDay, D5x), not
/// here: this binary trades exactly one pre-decided instrument against
/// exactly one pre-decided dataset, so there's no runtime flag for either.
/// `carry`'s symbol/spot_venue/futures_venue are filled in by run() (D48)
/// — only the runtime-swept threshold/size fields are meant to be set here.
/// This is also the shape a future pybind11 wrapper wants (see apps/
/// backtest's own design notes): a Python-side optimizer sweeps `carry`/
/// `risk`, never rebuilds the extension module to change them.
struct Config {
    strategy::carry::Config          carry{};
    risk::basic::BasicRiskGateConfig risk{};
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

/// Replays config::kDataDir's futures + spot files for config::kSymbol
/// over [config::kFirstDay, config::kLastDay] to completion, through the
/// same mechanism a live composition uses (D3): each leg's config::Source
/// feeds its own config::Sink, both driven by a driver thread exactly like
/// run_data_source's; Engine consumes both legs merged in timestamp order
/// via BacktestInProcessTransport. A ControlChannel coordinates the driver
/// thread's "both legs exhausted" signal with Engine's own driving loop —
/// Engine only exposes step(), so run() drives it directly against that
/// signal plus a grace window (TODO in backtest.cpp: placeholder pending
/// an app-level DataSource-liveness coordinator, see engine.hpp).
Results run(const Config& config);

}  // namespace qp::backtest
