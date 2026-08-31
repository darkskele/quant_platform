#pragma once
#include <span>
#include <utility>

#include "clock.hpp"
#include "execution_gateway.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "strategy.hpp"
#include "transport.hpp"
#include "types.hpp"

namespace qp::engine {

/// The trader composition root. One Strategy, one Risk, one shared Book.
/// Tx is transport::Transport, a driving loop only ever needs step(). 
template <transport::Transport Tx, Clock Clk, execution::ExecutionGateway Exec, risk::RiskGate Risk,
          strategy::Strategy S, PortfolioLike Book>
class Engine {
   public:
    Engine(Tx transport, Clk clock, Exec exec, Risk risk, S strategy, Book& portfolio)
        : transport_{std::move(transport)},
          clock_{std::move(clock)},
          exec_{std::move(exec)},
          risk_{std::move(risk)},
          strategy_{std::move(strategy)},
          state_{portfolio} {}

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
        return true;
    }

   private:
    void submit(const Order& order) { exec_.submit(order, clock_.now()); }

    void submit_if_approved(const risk::RiskDecision& decision) {
        if (decision.outcome == risk::RiskOutcome::Rejected) return;
        submit(*decision.order);
    }

    void drain_outcomes() {
        for (const auto& fill : exec_.fills()) state_.apply_fill(fill);
        // Reject: no Portfolio effect yet — exec_.rejects() is there when it is.
    }

    Tx    transport_;
    Clk   clock_;
    Exec  exec_;
    Risk  risk_;
    S     strategy_;
    Book& state_;
};

}  // namespace qp::engine
