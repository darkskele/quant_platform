#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "collector_venue.hpp"
#include "control_channel.hpp"
#include "venue_types.hpp"

namespace qp::collector {

struct Config {
    std::vector<std::string> symbols;  // same list applies to both legs (D41: carry's actual
                                       // need — matching underlying instruments on both markets)
    std::filesystem::path data_dir;    // root; recordings land in data_dir/futures/
                                       // and data_dir/spot/ (D41 — FileRecorder writes
                                       // data_dir/symbols.manifest unconditionally,
                                       // so two recorders can't share one data_dir)
    source::WsEndpoint                  ws_endpoint        = kDefaultWsEndpoint;
    source::RestEndpoint                rest_endpoint      = kDefaultRestEndpoint;
    source::WsEndpoint                  spot_ws_endpoint   = kDefaultSpotWsEndpoint;
    source::RestEndpoint                spot_rest_endpoint = kDefaultSpotRestEndpoint;
    std::optional<std::chrono::seconds> run_duration;  // nullopt = run indefinitely
};

/// Parses argv into a Config.
/// @return nullopt on bad/missing args (usage already printed to stderr) —
///     caller should exit non-zero in that case.
std::optional<Config> parse_args(int argc, char** argv);

/// Wires source -> recorder, prints a periodic status line, and stops when
/// either `control` broadcasts Stop or `config.run_duration` elapses,
/// whichever first — same ControlChannel-based stop signal apps/backtest
/// uses (D3/D5x), so a SIGINT/SIGTERM handler and a future live Engine
/// composition tear down through the identical mechanism, not a bespoke
/// one per app.
/// @param control this run()'s own poll loop is the "whichever loop is
///     already polling the channel" that pump()s it (ControlChannel's own
///     contract) — a caller that never calls request_stop() on it (e.g. a
///     test using a short run_duration instead) never needs to touch it
///     beyond attach()ing.
/// @param consumer this run()'s own index on `control`, from
///     ControlChannel::attach() — the caller attaches, not run() itself,
///     so the same channel can be attach()'d before any signal handler
///     that might race a very early request_stop() is installed.
int run(const Config& config, ControlChannel<1>& control, std::size_t consumer);

}  // namespace qp::collector
