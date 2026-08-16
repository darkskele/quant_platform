#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "venue.hpp"
#include "venue_types.hpp"

namespace qp::collector {

struct Config {
    std::vector<std::string>            symbols;
    std::filesystem::path               data_dir;
    source::WsEndpoint                  ws_endpoint   = kDefaultWsEndpoint;
    source::RestEndpoint                rest_endpoint = kDefaultRestEndpoint;
    std::optional<std::chrono::seconds> run_duration;  // nullopt = run indefinitely
};

/// Parses argv into a Config.
/// @return nullopt on bad/missing args (usage already printed to stderr) —
///     caller should exit non-zero in that case.
std::optional<Config> parse_args(int argc, char** argv);

/// Wires source -> recorder, prints a periodic status line, and stops when
/// either `stop_requested` becomes true or `config.run_duration` elapses,
/// whichever first.
/// @param stop_requested settable from a signal handler; testable directly
///     with a short `run_duration` and a stop_requested that's never set —
///     no real signals, no argv needed.
int run(const Config& config, std::atomic<bool>& stop_requested);

}  // namespace qp::collector
