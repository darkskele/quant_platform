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

// One leg's source+recorder pair, plus the retry-count bookkeeping the
// status/warning lines need — same shape run() used to inline for its one
// (now first of two) leg. Genuinely duplicated across futures+spot below
// (not speculative), so pulled out once rather than doubled inline.
template <source::Parser P, source::AlignmentRule Rule>
struct Leg {
    const char*                                 name;  // "futures" / "spot" — log prefix only
    source::GenericLiveWebSocketSource<P, Rule> source;
    sink::FileRecorder                          recorder;
    std::size_t                                 last_retry_count;

    Leg(const char* leg_name, const std::vector<std::string>& symbols,
        source::WsEndpoint ws_endpoint, source::RestEndpoint rest_endpoint,
        const std::filesystem::path& data_dir_root)
        : name(leg_name),
          source(symbols, ws_endpoint, rest_endpoint),
          recorder(data_dir_root / leg_name, source.symbol_names()),
          last_retry_count(source.resync_retry_count()) {}

    // Non-blocking poll; returns true if an event was recorded this call
    // (the caller only sleeps when every leg's poll came back empty).
    bool poll() {
        if (auto ev = source.next()) {
            recorder.record(std::move(*ev));
            return true;
        }
        return false;
    }

    // Aggregate, not per-symbol: LiveWebSocketSource's resync_retry_count()
    // doesn't break down which symbol is struggling (docs/decisions.md
    // D12). Rather than guess and mis-attribute a note to a symbol that
    // wasn't actually the one retrying, this alerts loudly (stderr)
    // without writing anything into a specific symbol's manifest.
    void log_status_if_due(std::chrono::steady_clock::time_point now,
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
};

}  // namespace

int run(const Config& config, std::atomic<bool>& stop_requested) {
    static constexpr auto kPollInterval = std::chrono::seconds(5);
    static constexpr auto kIdleSleep    = std::chrono::milliseconds(10);

    // source -> recorder directly, not an Engine (docs/architecture-principles.md).
    // Two legs, futures + spot (D41 — carry needs both), each independent:
    // own SymbolTable, own FileRecorder, own data_dir/<leg>/symbols.manifest.
    // Nothing merges them here — that's a backtest/live Engine wiring
    // concern (CombinedTransport), not a collector one; each leg staying
    // fully independent on disk is exactly what avoids the SymbolId
    // collision two venues interning the same string would otherwise hit.
    Leg<SelectedParser, SelectedAlignment> futures("futures", config.symbols, config.ws_endpoint,
                                                   config.rest_endpoint, config.data_dir);
    Leg<SelectedSpotParser, SelectedSpotAlignment> spot("spot", config.symbols,
                                                        config.spot_ws_endpoint,
                                                        config.spot_rest_endpoint, config.data_dir);

    const auto start = std::chrono::steady_clock::now();
    const auto deadline =
        config.run_duration ? std::optional(start + *config.run_duration) : std::nullopt;
    auto next_poll = start + kPollInterval;

    std::cerr << "[collector] starting: " << config.symbols.size() << " symbol(s) x 2 legs -> "
              << config.data_dir << " (futures/, spot/)\n";

    while (!stop_requested.load(std::memory_order_acquire)) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) break;

        // Both legs polled unconditionally every iteration (not `a || b`,
        // which would skip polling the second leg once the first found
        // something) — only sleep once neither leg had anything.
        bool futures_got_event = futures.poll();
        bool spot_got_event    = spot.poll();
        if (!futures_got_event && !spot_got_event) std::this_thread::sleep_for(kIdleSleep);

        auto now = std::chrono::steady_clock::now();
        if (now < next_poll) continue;
        next_poll = now + kPollInterval;

        futures.log_status_if_due(now, start);
        spot.log_status_if_due(now, start);
    }

    std::cerr << "[collector] stopping\n";
    // Clean shutdown either way: sources/recorders go out of scope here,
    // and their destructors drain their queues and close properly.
    return 0;
}

}  // namespace qp::collector
