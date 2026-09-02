#pragma once
#include <chrono>
#include <cstddef>
#include <span>
#include <thread>
#include <utility>

#include "clock.hpp"
#include "control_channel.hpp"
#include "execution_gateway.hpp"
#include "portfolio.hpp"
#include "recorder/null/null_recorder.hpp"
#include "recorder/recorder.hpp"
#include "risk_gate.hpp"
#include "strategy.hpp"
#include "transport.hpp"
#include "types.hpp"

namespace qp::engine {

/// The trader composition root. One Strategy, one Risk, one shared Book.
/// Tx is transport::Transport, a driving loop only ever needs step().
template <transport::Transport Tx, Clock Clk, execution::ExecutionGateway Exec, risk::RiskGate Risk,
          strategy::Strategy S, PortfolioLike Book, class Rec = NullRecorder>
    requires Recorder<Rec, Book>
class Engine {
   public:
    Engine(Tx transport, Clk clock, Exec exec, Risk risk, S strategy, Book& portfolio,
           Rec recorder = {})
        : transport_{std::move(transport)},
          clock_{std::move(clock)},
          exec_{std::move(exec)},
          risk_{std::move(risk)},
          strategy_{std::move(strategy)},
          state_{portfolio},
          recorder_{std::move(recorder)} {}

    /// Runs until told to stop on `control`. On Stop, flushes the transport
    /// and drains what's buffered before returning. Meant to be the body of
    /// its own thread.
    template <std::size_t NumControlConsumers>
    void run(ControlChannel<NumControlConsumers>& control, std::size_t consumer) {
        for (;;) {
            // Step a batch, then poll once: polling (or reading a clock) on
            // every event would tax the hot path, and shutdown latency isn't
            // critical. A plain counter, not steady_clock, gates the poll.
            bool progressed = false;
            for (std::size_t i = 0; i < kStepsPerPoll; ++i) {
                if (!step()) break;  // transport dry -> poll and back off
                progressed = true;
            }
            if (control.poll(consumer) == ControlCommand::Stop) break;
            if (!progressed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        transport_.flush();
        while (step()) {
        }
    }

    /// Processes exactly one pulled event. False means transport exhausted.
    bool step() {
        auto event = transport_.next();
        if (!event) return false;

        clock_.advance(header_of(*event).ts);
        exec_.on_market_event(*event);
        state_.apply_funding(*event);
        state_.apply_mark_price(*event);

        for (const auto& intent : strategy_.on_event(*event))
            submit_if_approved(risk_.check(intent));

        for (const auto& order : risk_.on_tick()) submit(order);

        drain_outcomes();
        recorder_.sample(clock_.now(), state_);  // equity once the event is fully applied
        return true;
    }

   private:
    // Steps between control-channel polls in run().
    static constexpr std::size_t kStepsPerPoll = 1 << 20;

    void submit(const Order& order) { exec_.submit(order, clock_.now()); }

    void submit_if_approved(const risk::RiskDecision& decision) {
        if (decision.outcome == risk::RiskOutcome::Rejected) return;
        submit(*decision.order);
    }

    void drain_outcomes() {
        for (const auto& fill : exec_.fills()) state_.apply_fill(fill);
        // Reject: no Portfolio effect yet, exec_.rejects() is there when it is.
    }

    Tx    transport_;
    Clk   clock_;
    Exec  exec_;
    Risk  risk_;
    S     strategy_;
    Book& state_;
    Rec   recorder_;
};

}  // namespace qp::engine
