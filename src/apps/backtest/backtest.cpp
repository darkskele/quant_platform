#include "backtest.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

#include "backtest_in_process_transport.hpp"
#include "clock.hpp"
#include "control_channel.hpp"
#include "engine.hpp"
#include "execution_gateway.hpp"
#include "fanout_sink.hpp"
#include "file_replay_source.hpp"
#include "matcher/last_trade/last_trade_matcher.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "sim_clock.hpp"
#include "sim_execution.hpp"
#include "strategy.hpp"

namespace qp::backtest {

namespace {

// D48: the collector's own pairing order (apps/collector/collector.cpp's
// run_data_source(sources, sinks, ...) tuple, D41/D43) stamps futures
// events venue=0, spot events venue=1 — this backtest replays those same
// two directories, so it has to reuse the same convention, not invent one.
constexpr VenueId kFuturesVenue = 0;
constexpr VenueId kSpotVenue    = 1;

// Sized for a research backtest's replay rate, not tuned against real
// numbers yet — same "placeholder, not validated" status as SpmcRing's own
// backoff constants until this path gets benchmarked for real.
constexpr std::size_t kRingCapacity = 1024;

void print_usage(const char* prog) {
    std::cerr
        << "usage: " << prog
        << " --data-dir DIR --symbol SYMBOL --first-day YYYY-MM-DD --last-day YYYY-MM-DD "
           "[--entry-rate RATE] [--exit-rate RATE] [--target-qty QTY] [--max-position QTY] "
           "[--max-drawdown NOTIONAL]\n"
        << "  --data-dir DIR     root containing data_dir/futures/ and data_dir/spot/ "
           "(apps/collector's D41 layout)\n"
        << "  --symbol SYMBOL    the one instrument to trade, e.g. BTCUSDT\n"
        << "  --first-day/--last-day  inclusive UTC date range to replay\n"
        << "  --entry-rate/--exit-rate/--target-qty   FundingCarryStrategy::Config overrides "
           "(default: placeholders, see D45)\n"
        << "  --max-position/--max-drawdown           BasicRiskGateConfig overrides (default: "
           "placeholders, see D47)\n";
}

// Manual parse, not std::chrono::parse: this repo's libstdc++ doesn't yet
// implement chrono's from_stream support, only formatting (format_day's
// std::format("{:%Y-%m-%d}", ...) already works — parsing is the gap).
std::optional<wire::DayKey> parse_day(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return std::nullopt;
    try {
        int                         y = std::stoi(s.substr(0, 4));
        int                         m = std::stoi(s.substr(5, 2));
        int                         d = std::stoi(s.substr(8, 2));
        std::chrono::year_month_day ymd{std::chrono::year{y}, std::chrono::month{unsigned(m)},
                                        std::chrono::day{unsigned(d)}};
        if (!ymd.ok()) return std::nullopt;
        return std::chrono::sys_days{ymd}.time_since_epoch().count();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

SymbolId read_symbol_id_from_manifest(const std::filesystem::path& manifest_path,
                                      const std::string&           symbol_name) {
    std::ifstream in(manifest_path);
    if (!in) throw std::runtime_error("missing manifest: " + manifest_path.string());

    std::string line;
    SymbolId    id = 0;
    while (std::getline(in, line)) {
        if (line == symbol_name) return id;
        ++id;
    }
    throw std::runtime_error("symbol \"" + symbol_name + "\" not found in " +
                             manifest_path.string());
}

// D48: both legs' recorders independently intern the same config.symbols
// list, in the same order (apps/collector's D41) — so both manifests
// *should* assign the same SymbolId to the same name, but nothing in the
// file format enforces that. Cross-checked here rather than assumed: a
// silent mismatch would make FundingCarryStrategy read one leg's BTCUSDT
// as the other leg's ETHUSDT.
SymbolId resolve_symbol_id(const std::filesystem::path& data_dir, const std::string& symbol_name) {
    SymbolId futures_id =
        read_symbol_id_from_manifest(data_dir / "futures" / "symbols.manifest", symbol_name);
    SymbolId spot_id =
        read_symbol_id_from_manifest(data_dir / "spot" / "symbols.manifest", symbol_name);
    if (futures_id != spot_id) {
        throw std::runtime_error("symbol id mismatch between futures/spot manifests for \"" +
                                 symbol_name + "\" (futures=" + std::to_string(futures_id) +
                                 ", spot=" + std::to_string(spot_id) + ")");
    }
    return futures_id;
}

}  // namespace

std::optional<Config> parse_args(int argc, char** argv) {
    Config config;
    bool   have_first_day = false;
    bool   have_last_day  = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        auto next_value = [&]() -> std::optional<std::string> {
            if (++i >= argc) return std::nullopt;
            return std::string{argv[i]};
        };

        if (arg == "--data-dir") {
            auto v = next_value();
            if (!v) {
                print_usage(argv[0]);
                return std::nullopt;
            }
            config.data_dir = *v;
        } else if (arg == "--symbol") {
            auto v = next_value();
            if (!v) {
                print_usage(argv[0]);
                return std::nullopt;
            }
            config.symbol = *v;
        } else if (arg == "--first-day" || arg == "--last-day") {
            auto v   = next_value();
            auto day = v ? parse_day(*v) : std::nullopt;
            if (!day) {
                std::cerr << "invalid " << arg << ": " << (v ? *v : "") << "\n";
                return std::nullopt;
            }
            (arg == "--first-day" ? config.first_day : config.last_day) = *day;
            (arg == "--first-day" ? have_first_day : have_last_day)     = true;
        } else if (arg == "--entry-rate" || arg == "--exit-rate" || arg == "--target-qty" ||
                   arg == "--max-position" || arg == "--max-drawdown") {
            auto v = next_value();
            if (!v) {
                print_usage(argv[0]);
                return std::nullopt;
            }
            try {
                double value = std::stod(*v);
                if (arg == "--entry-rate")
                    config.carry.entry_funding_rate = value;
                else if (arg == "--exit-rate")
                    config.carry.exit_funding_rate = value;
                else if (arg == "--target-qty")
                    config.carry.target_qty = value;
                else if (arg == "--max-position")
                    config.risk.max_position_qty = value;
                else
                    config.risk.max_drawdown = value;
            } catch (const std::exception&) {
                std::cerr << "invalid " << arg << ": " << *v << "\n";
                return std::nullopt;
            }
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return std::nullopt;
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return std::nullopt;
        }
    }

    if (config.data_dir.empty() || config.symbol.empty() || !have_first_day || !have_last_day) {
        std::cerr << "--data-dir, --symbol, --first-day, and --last-day are all required\n";
        print_usage(argv[0]);
        return std::nullopt;
    }

    return config;
}

Results run(const Config& config) {
    SymbolId symbol = resolve_symbol_id(config.data_dir, config.symbol);

    source::FileReplaySource futures_source{config.data_dir / "futures", config.first_day,
                                            config.last_day,
                                            std::vector<std::string>{config.symbol}};
    source::FileReplaySource spot_source{config.data_dir / "spot", config.first_day,
                                         config.last_day, std::vector<std::string>{config.symbol}};

    strategy::carry::Config carry_config = config.carry;
    carry_config.symbol                  = symbol;
    carry_config.futures_venue           = kFuturesVenue;
    carry_config.spot_venue              = kSpotVenue;

    risk::basic::BasicRiskGateConfig risk_config = config.risk;
    risk_config.tracked                          = {{symbol, kSpotVenue}, {symbol, kFuturesVenue}};

    using FanSink = sink::FanoutSink<kRingCapacity, 1>;  // one consumer: this backtest's own Engine
    FanSink     futures_fanout;
    FanSink     spot_fanout;
    std::size_t futures_consumer = futures_fanout.attach();
    std::size_t spot_consumer    = spot_fanout.attach();

    ControlChannel<1> control;  // one attached participant: Engine's own transport
    std::size_t       engine_ctrl_idx = control.attach();

    // Drives both legs into their rings on its own thread, exactly like
    // run_data_source does for collector — same mechanism, backtest and
    // live alike (D3). Unlike run_data_source (which runs until an
    // external stop_requested), this loop knows FileReplaySource's own
    // next()==nullopt is *permanent* end-of-data (unlike a live source's,
    // which just means "queue empty right now" — see file_replay_source.hpp),
    // so it can detect true exhaustion itself and request_stop() once both
    // legs are done, closing the loop BacktestInProcessTransport needs.
    std::thread driver([&] {
        bool futures_done = false;
        bool spot_done    = false;
        while (!(futures_done && spot_done)) {
            bool any = false;
            if (!futures_done) {
                if (auto ev = futures_source.next()) {
                    ev->venue = kFuturesVenue;
                    futures_fanout.record(std::move(*ev));
                    any = true;
                } else {
                    futures_done = true;
                }
            }
            if (!spot_done) {
                if (auto ev = spot_source.next()) {
                    ev->venue = kSpotVenue;
                    spot_fanout.record(std::move(*ev));
                    any = true;
                } else {
                    spot_done = true;
                }
            }
            if (!any && !(futures_done && spot_done)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        control.request_stop();
    });

    using Tx = transport::BacktestInProcessTransport<FanSink::Ring, 2, 1>;
    Tx transport({&futures_fanout.ring(), &spot_fanout.ring()}, {futures_consumer, spot_consumer},
                 control, engine_ctrl_idx);

    using Exec =
        execution::sim::SimExecution<execution::sim::matcher::last_trade::LastTradeMatcher>;
    using Risk = risk::basic::BasicRiskGate<Portfolio>;

    Portfolio portfolio;
    Risk      risk_gate{risk_config, portfolio};

    Engine<Tx, SimClock, Exec, Risk, strategy::carry::FundingCarryStrategy, Portfolio> engine{
        std::move(transport),
        SimClock{},
        Exec{},
        std::move(risk_gate),
        strategy::carry::FundingCarryStrategy{carry_config},
        portfolio};

    // Not engine.run(): its plain while(step()){} stops on the first
    // nullopt, which for a ring-fed Transport can mean "nothing right now"
    // as easily as "genuinely done". is_done() is the ring/ControlChannel-
    // aware signal that actually distinguishes them (see backtest.hpp).
    while (!engine.transport().is_done()) {
        if (!engine.step()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    driver.join();

    StateView state = engine.view();
    return Results{.final_cash             = state.cash(),
                   .final_equity           = state.equity(),
                   .final_spot_position    = state.position(symbol, kSpotVenue),
                   .final_futures_position = state.position(symbol, kFuturesVenue)};
}

}  // namespace qp::backtest
