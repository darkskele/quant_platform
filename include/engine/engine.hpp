#pragma once
#include <cstddef>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "clock.hpp"
#include "execution_gateway.hpp"
#include "portfolio.hpp"
#include "risk_gate.hpp"
#include "round_robin_pool.hpp"
#include "strategy.hpp"
#include "transport.hpp"
#include "types.hpp"

namespace qp {

/// The trader composition root (D27). Each Strategy runs on a
/// shared RoundRobinPool (D33) — risk-check/submit stays single-threaded,
/// strategy-index-ordered, so a backtest's decision sequence never depends
/// on thread scheduling. Tx is transport::Transport (D22/D38), not
/// source::Source — Engine consumes whatever hands it MarketEvents one at a
/// time (a live/replay Source directly, an InProcessTransport over a
/// fan-out ring, or a CombinedTransport merging several legs), not
/// specifically a "source".
template <transport::Transport Tx, Clock Clk, execution::ExecutionGateway Exec, risk::RiskGate Risk,
          std::size_t NumWorkers, Strategy... Strategies>
class Engine {
    struct EventContext {
        const MarketEvent& event;
        StateView          state;
    };

    // Adapts one Strategy (2-arg on_event) to the pool's 1-arg Task shape
    // (Result operator()(const Context&)) — holds a pointer, not the
    // strategy itself, so it stays cheap to construct per Engine instance.
    template <class S>
    struct StrategyTask {
        S* strategy;

        std::vector<Intent> operator()(const EventContext& ctx) {
            return strategy->on_event(ctx.event, ctx.state);
        }
    };

    using Pool =
        RoundRobinPool<NumWorkers, EventContext, std::vector<Intent>, StrategyTask<Strategies>...>;

   public:
    Engine(Tx transport, Clk clock, Exec exec, Risk risk, Strategies... strategies)
        : transport_{std::move(transport)},
          clock_{std::move(clock)},
          exec_{std::move(exec)},
          risk_{std::move(risk)},
          strategies_{std::move(strategies)...},
          pool_{make_pool(std::index_sequence_for<Strategies...>{})} {}

    /// Processes exactly one pulled event; false = transport exhausted.
    bool step() {
        auto event = transport_.next();
        if (!event) return false;

        clock_.advance(event->ts);
        exec_.on_market_event(*event);
        if (event->kind == EventKind::Funding) state_.apply_funding(*event);
        state_.apply_mark_price(*event);

        EventContext ctx{*event, state_.view()};
        pool_.run_round(ctx, [&](std::size_t, std::vector<Intent> intents) {
            for (const auto& intent : intents)
                submit_if_approved(risk_.check(intent, state_.view()));
        });

        for (const auto& order : risk_.on_tick(state_.view())) submit(order);

        drain_outcomes();
        return true;
    }

    void run() {
        while (step()) {
        }
    }

    StateView view() const noexcept { return state_.view(); }

    /// Direct access to the owned Transport — for a driving loop that
    /// needs to distinguish "nothing right now" from "genuinely finished"
    /// itself (e.g. BacktestInProcessTransport::is_done()), which run()'s
    /// own while(step()){} can't: it treats any false from step() as done,
    /// correct only for a Transport whose next() is permanently nullopt
    /// once exhausted.
    Tx& transport() noexcept { return transport_; }

   private:
    template <std::size_t... Is>
    Pool make_pool(std::index_sequence<Is...>) {
        return Pool{StrategyTask<Strategies>{&std::get<Is>(strategies_)}...};
    }

    void submit(const Order& order) { exec_.submit(order, clock_.now()); }

    void submit_if_approved(const risk::RiskDecision& decision) {
        if (decision.outcome == risk::RiskOutcome::Rejected) return;
        submit(*decision.order);
    }

    void drain_outcomes() {
        while (auto outcome = exec_.next_outcome())
            if (auto* fill = std::get_if<Fill>(&*outcome)) state_.apply_fill(*fill);
        // Reject: no Portfolio effect yet.
    }

    Tx   transport_;
    Clk  clock_;
    Exec exec_;
    Risk risk_;
    std::tuple<Strategies...>
              strategies_;  // must precede pool_: make_pool() takes addresses into it
    Portfolio state_;
    Pool      pool_;
};

}  // namespace qp
