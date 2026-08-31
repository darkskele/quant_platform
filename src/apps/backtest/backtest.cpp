#include "backtest.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

#include "config/compose.hpp"
#include "control_channel.hpp"

namespace qp::backtest {

using namespace qp::data_source;

namespace {

// D48: the collector's own pairing order (apps/collector/collector.cpp's
// run_data_source(sources, sinks, ...) tuple, D41/D43) stamps futures
// events venue=0, spot events venue=1 — matches config::VenueTables'
// declaration order (venues.hpp), which is what sizes config::Book.
constexpr VenueId kFuturesVenue = 0;
constexpr VenueId kSpotVenue    = 1;

void print_usage(const char* prog) {
    std::cerr << "usage: " << prog
              << " [--entry-rate RATE] [--exit-rate RATE] [--target-qty QTY] [--max-position "
                 "QTY] [--max-drawdown NOTIONAL]\n"
              << "  Symbol, dataset, and Source/Sink/Matcher/RiskGate/Strategy types are all "
                 "decided at build time (see tools/build_backtest.sh, include/apps/backtest/"
                 "config/) — not by flags.\n"
              << "  --entry-rate/--exit-rate/--target-qty   FundingCarryStrategy::Config overrides "
                 "(default: placeholders, see D45)\n"
              << "  --max-position/--max-drawdown           BasicRiskGateConfig overrides "
                 "(default: placeholders, see D47)\n";
}

// One leg's whole production loop, on its own thread. Each leg gets its
// *own* thread — not one thread interleaving both, which was the original
// shape here — because config::Sink::record() can genuinely block
// (SpmcRing::push() backpressures on a full ring, FanoutSink's own
// contract) and BacktestInProcessTransport won't release either leg's
// buffered lookahead until *both* have something to compare timestamps
// against. One combined thread blocked pushing into leg A's full ring
// never reaches leg B's next()/record() either — Transport is left
// waiting on B forever, so it never drains A's ring again, so the thread
// stays blocked on A forever: a real deadlock, reproduced with 2 days of
// real Binance data (~1024 events, one ring's capacity) even though a
// smaller smoke test never triggered it. Two independent threads mean a
// full ring on one leg only stalls that leg's own thread.
template <class Source, class Sink>
void drive_leg(Source& source, Sink& sink, VenueId venue) {
    while (true) {
        if (auto ev = source.next()) {
            std::visit([venue](auto& e) { e.venue = venue; }, *ev);
            sink.record(std::move(*ev));
        } else if (source.is_done()) {
            return;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

}  // namespace

std::optional<Config> parse_args(int argc, char** argv) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        auto next_value = [&]() -> std::optional<std::string> {
            if (++i >= argc) return std::nullopt;
            return std::string{argv[i]};
        };

        if (arg == "--entry-rate" || arg == "--exit-rate" || arg == "--target-qty" ||
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

    return config;
}

Results run(const Config& config) {
    strategy::carry::Config carry_config = config.carry;
    carry_config.symbol                  = config::kSymbolId;
    carry_config.futures_venue           = kFuturesVenue;
    carry_config.spot_venue              = kSpotVenue;

    risk::basic::BasicRiskGateConfig risk_config = config.risk;
    risk_config.tracked = {{config::kSymbolId, kSpotVenue}, {config::kSymbolId, kFuturesVenue}};

    config::FuturesSource futures_source = config::make_futures_source(
        config::data_dir(), config::kSymbol, config::kFirstDay, config::kLastDay);
    config::SpotSource spot_source = config::make_spot_source(config::data_dir(), config::kSymbol,
                                                              config::kFirstDay, config::kLastDay);

    config::Sink futures_fanout;
    config::Sink spot_fanout;
    // NumConsumers == 1 on both legs, so the sole consumer's ring index is
    // trivially 0.
    std::size_t futures_consumer = 0;
    std::size_t spot_consumer    = 0;

    ControlChannel<1> control;  // one attached participant: Engine's own transport
    std::size_t       engine_ctrl_idx = control.attach();

    // See drive_leg's own comment for why this is two threads, not one.
    std::thread futures_driver([&] { drive_leg(futures_source, futures_fanout, kFuturesVenue); });
    std::thread spot_driver([&] { drive_leg(spot_source, spot_fanout, kSpotVenue); });

    // Signals Engine's driving loop once both legs are genuinely exhausted
    // — a third, otherwise-idle thread that just joins the two producers,
    // rather than either producer thread polling the other's completion
    // (which would reintroduce the same cross-leg coupling drive_leg's
    // split was meant to remove).
    std::atomic<bool> data_exhausted{false};
    std::thread       stop_signaler([&] {
        futures_driver.join();
        spot_driver.join();
        control.request_stop();
        data_exhausted.store(true, std::memory_order_release);
    });

    config::Tx transport({&futures_fanout.ring(), &spot_fanout.ring()},
                         {futures_consumer, spot_consumer}, control, engine_ctrl_idx);

    config::Book     portfolio;
    config::Risk     risk_gate{risk_config, portfolio};
    config::Strategy carry_strategy{carry_config, portfolio};

    config::EngineType engine{
        std::move(transport),      SimClock{}, config::Exec{}, std::move(risk_gate),
        std::move(carry_strategy), portfolio};

    // TODO: Engine no longer exposes a transport()/is_done() getter (it
    // never forwards Book, and shouldn't forward Tx either — a driving
    // loop only needs step()). The real replacement is an app-level
    // coordinator that watches DataSource liveness directly and issues
    // Stop via ControlChannel when a source dies (engine.hpp), not built
    // yet. Interim placeholder: drive step() until both legs' own "done"
    // signal fires, then keep draining for a grace window comfortably
    // longer than ControlChannel's ~100ms pump interval, so Stop has time
    // to actually propagate and flush whatever's still buffered downstream
    // of it. Simpler than the old is_done() check but can't distinguish a
    // truly stalled leg from one still catching up within the grace
    // window — acceptable for now, not for a production
    // DataSource-liveness design.
    while (!data_exhausted.load(std::memory_order_acquire)) {
        if (!engine.step()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto grace_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (std::chrono::steady_clock::now() < grace_until) {
        if (!engine.step()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    stop_signaler.join();

    return Results{.final_cash             = portfolio.cash(),
                   .final_equity           = portfolio.equity(),
                   .final_spot_position    = portfolio.position(config::kSymbolId, kSpotVenue),
                   .final_futures_position = portfolio.position(config::kSymbolId, kFuturesVenue)};
}

}  // namespace qp::backtest
