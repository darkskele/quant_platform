#include "collector.hpp"

#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#include "file_recorder.hpp"
#include "live_websocket_source.hpp"
#include "sink.hpp"

namespace qp::collector {

namespace {

void print_usage(const char* prog) {
    std::cerr << "usage: " << prog
              << " SYMBOL [SYMBOL...] --data-dir DIR [--duration SECONDS] [--testnet]\n"
              << "  SYMBOL          Binance symbol(s) to record, e.g. BTCUSDT ETHUSDT\n"
              << "  --data-dir DIR  local directory for recorded partitions (required)\n"
              << "  --duration SEC  stop after SEC seconds; omit to run until SIGINT/SIGTERM\n"
              << "  --testnet       use Binance's futures testnet instead of production\n";
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
        config.ws_endpoint   = kDefaultWsTestnet;
        config.rest_endpoint = kDefaultRestTestnet;
    }

    return config;
}

int run(const Config& config, std::atomic<bool>& stop_requested) {
    static constexpr auto kPollInterval = std::chrono::seconds(5);
    static constexpr auto kIdleSleep    = std::chrono::milliseconds(10);

    // source -> recorder directly, not an Engine (docs/architecture-principles.md);
    // SelectedParser/SelectedAlignment are venue.hpp's compile-time picks
    // (QP_COLLECTOR_VENUE).
    source::GenericLiveWebSocketSource<SelectedParser, SelectedAlignment> source(
        config.symbols, config.ws_endpoint, config.rest_endpoint);
    sink::Sink auto recorder = sink::FileRecorder(config.data_dir, source.symbol_names());

    const auto start = std::chrono::steady_clock::now();
    const auto deadline =
        config.run_duration ? std::optional(start + *config.run_duration) : std::nullopt;
    auto next_poll = start + kPollInterval;

    // Aggregate, not per-symbol: LiveWebSocketSource's resync_retry_count()
    // doesn't break down which symbol is struggling (see docs/decisions.md
    // D12's own discussion of this). Rather than guess and mis-attribute a
    // note to a symbol that wasn't actually the one retrying, this alerts
    // loudly (stderr) without writing anything into a specific symbol's
    // manifest. FileRecorder::log() stays available for whenever
    // LiveWebSocketSource exposes real per-symbol attribution.
    std::size_t last_retry_count = source.resync_retry_count();

    std::cerr << "[collector] starting: " << config.symbols.size() << " symbol(s) -> "
              << config.data_dir << "\n";

    while (!stop_requested.load(std::memory_order_acquire)) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) break;

        if (auto ev = source.next()) {
            recorder.record(std::move(*ev));
        } else {
            std::this_thread::sleep_for(kIdleSleep);
        }

        auto now = std::chrono::steady_clock::now();
        if (now < next_poll) continue;
        next_poll = now + kPollInterval;

        std::size_t retry_count = source.resync_retry_count();
        if (retry_count != last_retry_count) {
            std::cerr << "[collector] WARNING: resync_retry_count=" << retry_count << " (delta +"
                      << (retry_count - last_retry_count)
                      << ": rest_failures=" << source.resync_rest_failure_count()
                      << " buffer_overflows=" << source.resync_buffer_overflow_count()
                      << " no_alignment=" << source.resync_no_alignment_count()
                      << ") — a resync is not self-healing cleanly\n";
            last_retry_count = retry_count;
        }

        auto uptime_s = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        std::cerr << "[collector] status: uptime=" << uptime_s << "s"
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

    std::cerr << "[collector] stopping\n";
    // Clean shutdown either way: source/recorder go out of scope here, and
    // their destructors drain their queues and close properly.
    return 0;
}

}  // namespace qp::collector
