#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>
#include <thread>
#include <utility>

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

namespace detail {
/// One PAUSE-class hint: yields the core's pipeline to its SMT sibling for a
/// spin iteration without giving up the OS timeslice a yield() would.
inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#endif
}
}  // namespace detail

/// The trader composition root..
template <transport::Transport Tx, execution::ExecutionGateway Exec, risk::RiskGate Risk,
          strategy::Strategy S, PortfolioLike Book, Recorder<Book> Rec = NullRecorder>
class Engine {
   public:
    Engine(Tx transport, Exec exec, Risk risk, S strategy, Book& portfolio, Rec recorder = {})
        : transport_{std::move(transport)},
          exec_{std::move(exec)},
          risk_{std::move(risk)},
          strategy_{std::move(strategy)},
          state_{portfolio},
          recorder_{std::move(recorder)} {}

    /// Runs until told to stop on `control`. On Stop, flushes the transport
    /// and drains what's buffered before returning. Meant to be the body of
    /// its own thread.
    template <std::size_t NumControlConsumers>
    void run(ControlChannel<NumControlConsumers>& control, std::size_t consumer,
             std::chrono::microseconds idle_sleep = std::chrono::milliseconds(1)) {
        std::size_t idle = 0;  // consecutive dry outer loops, drives the backoff
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
            if (progressed)
                idle = 0;
            else
                back_off(idle++, idle_sleep);
        }
        transport_.flush();
        while (step()) {
        }
    }

    /// Processes exactly one pull. False means the transport is exhausted.
    bool step() {
        auto in = transport_.next();
        if (!in) return false;

        if (in->event) {
            const MarketEvent& event = *in->event;
            exec_.on_market_event(event);
            state_.apply_funding(event);
            state_.apply_mark_price(event);
            for (const auto& intent : strategy_.on_event(event))
                submit_if_approved(risk_.check(intent), in->ts);
        } else {
            for (const auto& intent : strategy_.on_timer(in->ts))
                submit_if_approved(risk_.check(intent), in->ts);
        }

        for (const auto& order : risk_.on_tick()) submit(order, in->ts);

        drain_outcomes();
        recorder_.sample(in->ts, state_);  // equity once the pull is fully applied
        return true;
    }

   private:
    // Steps between control-channel polls in run().
    static constexpr std::size_t kStepsPerPoll = 1 << 20;

    // Backoff schedule while the transport is dry. Not tuned against real
    // replay throughput yet. @todo
    static constexpr std::size_t               kSpinRounds  = 64;   // busy-spin, lowest latency
    static constexpr std::size_t               kYieldRounds = 256;  // then yield the timeslice
    static constexpr std::chrono::microseconds kMinNap{1};  // then sleep, doubling to idle_sleep

    /// Escalating idle backoff: spin, then yield, then a sleep that doubles
    /// up to idle_sleep. idle_sleep 0 never sleeps, a backtest replay blasts
    /// through transient source starvation on spin then yield.
    void back_off(std::size_t idle, std::chrono::microseconds idle_sleep) const {
        if (idle < kSpinRounds) {
            detail::cpu_relax();
        } else if (idle_sleep == std::chrono::microseconds::zero() ||
                   idle < kSpinRounds + kYieldRounds) {
            std::this_thread::yield();
        } else {
            std::size_t shift = std::min<std::size_t>(idle - (kSpinRounds + kYieldRounds), 20);
            std::chrono::microseconds nap{kMinNap.count() << shift};
            std::this_thread::sleep_for(std::min(idle_sleep, nap));
        }
    }

    void submit(const Order& order, Timestamp ts) { exec_.submit(order, ts); }

    void submit_if_approved(const risk::RiskDecision& decision, Timestamp ts) {
        if (decision.outcome == risk::RiskOutcome::Rejected) return;
        submit(*decision.order, ts);
    }

    void drain_outcomes() {
        for (const auto& fill : exec_.fills()) state_.apply_fill(fill);
        // Reject: no Portfolio effect yet, exec_.rejects() is there when it is.
    }

    Tx    transport_;
    Exec  exec_;
    Risk  risk_;
    S     strategy_;
    Book& state_;
    Rec   recorder_;
};

}  // namespace qp::engine
