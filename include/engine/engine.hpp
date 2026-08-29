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

/// The trader composition root (D27). One Strategy, one Risk, one shared
/// Book. Previously a variadic Strategies... pack running on a shared
/// RoundRobinPool (D33) — removed: nothing in this codebase ever
/// instantiated more than one Strategy except to exercise the pool's own
/// concurrency, and paying a cross-thread handshake (generation-counter
/// wakeup, SpscQueue round-trip) every step() for that never-otherwise-used
/// parallelism cost more than it saved (BENCHMARKS.md:
/// TwoNoopStrategiesTwoWorkers is *slower* than OneWorker for identical
/// work). "Several strategies sharing one account's risk/equity" — the
/// real reason the pack existed — is now a composition-root concern:
/// run several single-Strategy Engines against one shared Book&, on
/// however many threads (or none) the composition root chooses. Engine no
/// longer has an opinion.
///
/// Book is PortfolioLike, not the concrete Portfolio, and held by
/// reference, not owned — same "seam, not concrete adapter" discipline as
/// every other Engine dependency (D27), extended to account state. A
/// paired RiskGate/Strategy gets its own reference to the identical Book
/// at its own construction, wired by the composition root — Engine never
/// forwards it, and never exposes a getter back onto it either: nothing
/// outside this class reads state through Engine, the composition root
/// already holds the same Book reference it handed to everyone else.
///
/// Tx is transport::Transport (D22/D38), not source::Source — Engine
/// consumes whatever hands it MarketEvents one at a time (a live/replay
/// Source directly, or a BacktestInProcessTransport merging several
/// fan-out rings in timestamp order), not specifically a "source". No
/// run() and no transport() getter either — a driving loop only ever
/// needs step(); "keep going until genuinely done" is the composition
/// root's own call to make (it's the one that knows what "done" means for
/// its own Tx), not something to reach back into Engine for.
///
/// TODO: apps/backtest's driving loop currently has no way to ask "is the
/// underlying data source still alive" without a transport getter (which
/// this class deliberately doesn't have). The real fix is an app-level
/// coordinator that watches DataSource liveness directly and issues Stop
/// via ControlChannel when a source dies, sitting beside Engine rather
/// than reaching into it — not built yet.
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

    /// Processes exactly one pulled event; false = transport exhausted.
    bool step() {
        auto event = transport_.next();
        if (!event) return false;

        clock_.advance(event->ts);
        exec_.on_market_event(*event);
        if (event->kind == EventKind::Funding) state_.apply_funding(*event);
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
