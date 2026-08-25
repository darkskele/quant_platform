#include "collector.hpp"

#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <tuple>

#include "file_recorder.hpp"
#include "live_websocket_source.hpp"
#include "run_data_source.hpp"

namespace qp::collector {

namespace {

void print_usage(const char* prog) {
    std::cerr << "usage: " << prog
              << " SYMBOL [SYMBOL...] --data-dir DIR [--duration SECONDS] [--testnet]\n"
              << "  SYMBOL          Binance symbol(s) to record, e.g. BTCUSDT ETHUSDT\n"
              << "  --data-dir DIR  local directory for recorded partitions (required)\n"
              << "  --duration SEC  stop after SEC seconds; omit to run until SIGINT/SIGTERM\n"
              << "  --testnet       use Binance's testnet (both legs) instead of production\n";
}

}  // namespace

std::optional<Config> parse_args(int argc, char** argv) {
    Config config;
    bool   testnet = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--data-dir") {
            if (++i >= argc) {
                print_usage(argv[0]);
                return std::nullopt;
            }
            config.data_dir = argv[i];
        } else if (arg == "--duration") {
            if (++i >= argc) {
                print_usage(argv[0]);
                return std::nullopt;
            }
            try {
                config.run_duration = std::chrono::seconds(std::stoll(argv[i]));
            } catch (const std::exception&) {
                std::cerr << "invalid --duration: " << argv[i] << "\n";
                return std::nullopt;
            }
        } else if (arg == "--testnet") {
            testnet = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return std::nullopt;
        } else if (arg.starts_with("--")) {
            std::cerr << "unknown flag: " << arg << "\n";
            print_usage(argv[0]);
            return std::nullopt;
        } else {
            config.symbols.emplace_back(arg);
        }
    }

    if (config.symbols.empty()) {
        std::cerr << "no symbols given\n";
        print_usage(argv[0]);
        return std::nullopt;
    }
    if (config.data_dir.empty()) {
        std::cerr << "--data-dir is required\n";
        print_usage(argv[0]);
        return std::nullopt;
    }

    if (testnet) {
        config.ws_endpoint        = kDefaultWsTestnet;
        config.rest_endpoint      = kDefaultRestTestnet;
        config.spot_ws_endpoint   = kDefaultSpotWsTestnet;
        config.spot_rest_endpoint = kDefaultSpotRestTestnet;
    }

    return config;
}

namespace {

// Aggregate, not per-symbol: LiveWebSocketSource's resync_retry_count()
// doesn't break down which symbol is struggling (docs/decisions.md D12).
// Rather than guess and mis-attribute a note to a symbol that wasn't
// actually the one retrying, this alerts loudly (stderr) without writing
// anything into a specific symbol's manifest.
template <class SourceT, class RecorderT>
void log_status_if_due(const char* name, SourceT& source, RecorderT& recorder,
                       std::size_t& last_retry_count, std::chrono::steady_clock::time_point now,
                       std::chrono::steady_clock::time_point start) {
    std::size_t retry_count = source.resync_retry_count();
    if (retry_count != last_retry_count) {
        std::cerr << "[collector:" << name << "] WARNING: resync_retry_count=" << retry_count
                  << " (delta +" << (retry_count - last_retry_count)
                  << ": rest_failures=" << source.resync_rest_failure_count()
                  << " buffer_overflows=" << source.resync_buffer_overflow_count()
                  << " no_alignment=" << source.resync_no_alignment_count()
                  << ") — a resync is not self-healing cleanly\n";
        last_retry_count = retry_count;
    }

    auto uptime_s = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
    std::cerr << "[collector:" << name << "] status: uptime=" << uptime_s << "s"
              << " gaps=" << source.gap_count() << " resyncs=" << source.resync_count()
              << " retries=" << source.resync_retry_count()
              << " (rest=" << source.resync_rest_failure_count()
              << " overflow=" << source.resync_buffer_overflow_count()
              << " no_align=" << source.resync_no_alignment_count() << ")"
              << " dropped=" << source.dropped_count()
              << " queue_hwm=" << source.output_queue_high_water_mark()
              << " rec_dropped=" << recorder.dropped_count()
              << " rec_queue_hwm=" << recorder.queue_high_water_mark()
              << " rec_write_errors=" << recorder.write_error_count() << "\n";
}

}  // namespace

int run(const Config& config, std::atomic<bool>& stop_requested) {
    static constexpr auto kPollInterval = std::chrono::seconds(5);
    static constexpr auto kWatcherSleep = std::chrono::milliseconds(50);

    // source -> recorder directly, not an Engine (docs/architecture-principles.md).
    // Two legs, futures + spot (D41 — carry needs both), each independent:
    // own SymbolTable, own FileRecorder, own data_dir/<leg>/symbols.manifest.
    // Nothing merges them here — that's a backtest/live Engine wiring
    // concern (CombinedTransport), not a collector one; each leg staying
    // fully independent on disk is exactly what avoids the SymbolId
    // collision two venues interning the same string would otherwise hit.
    source::GenericLiveWebSocketSource<SelectedParser, SelectedAlignment> futures_source(
        config.symbols, config.ws_endpoint, config.rest_endpoint);
    sink::FileRecorder futures_recorder(config.data_dir / "futures", futures_source.symbol_names());

    source::GenericLiveWebSocketSource<SelectedSpotParser, SelectedSpotAlignment> spot_source(
        config.symbols, config.spot_ws_endpoint, config.spot_rest_endpoint);
    sink::FileRecorder spot_recorder(config.data_dir / "spot", spot_source.symbol_names());

    // run_data_source (libs/data_source, D44) owns the poll/record loop —
    // std::tie, not owning tuples: neither GenericLiveWebSocketSource nor
    // FileRecorder is movable/copyable, so the tuples just reference the
    // locals above, never construct/move them. Runs on its own thread so
    // this one is free to watch stop_requested/the --duration deadline and
    // print periodic status — concerns run_data_source deliberately knows
    // nothing about (it's generic over any Source/Sink, not specifically
    // GenericLiveWebSocketSource/FileRecorder's own counters).
    auto sources = std::tie(futures_source, spot_source);
    auto sinks   = std::tie(futures_recorder, spot_recorder);

    std::atomic<bool> running{true};
    std::thread       driver([&] { run_data_source(sources, sinks, running); });

    const auto start = std::chrono::steady_clock::now();
    const auto deadline =
        config.run_duration ? std::optional(start + *config.run_duration) : std::nullopt;
    auto next_poll = start + kPollInterval;

    std::size_t futures_last_retry = futures_source.resync_retry_count();
    std::size_t spot_last_retry    = spot_source.resync_retry_count();

    std::cerr << "[collector] starting: " << config.symbols.size() << " symbol(s) x 2 legs -> "
              << config.data_dir << " (futures/, spot/)\n";

    while (!stop_requested.load(std::memory_order_acquire)) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) break;

        auto now = std::chrono::steady_clock::now();
        if (now < next_poll) {
            std::this_thread::sleep_for(kWatcherSleep);
            continue;
        }
        next_poll = now + kPollInterval;

        log_status_if_due("futures", futures_source, futures_recorder, futures_last_retry, now,
                          start);
        log_status_if_due("spot", spot_source, spot_recorder, spot_last_retry, now, start);
    }

    running.store(false, std::memory_order_release);
    driver.join();

    std::cerr << "[collector] stopping\n";
    // Clean shutdown either way: sources/recorders go out of scope here,
    // and their destructors drain their queues and close properly.
    return 0;
}

}  // namespace qp::collector
