# engine

## Diagram

```
Engine<Tx, Clk, Exec, Risk, NumWorkers, Strategies...>::step()

  Tx::next() ──▶ MarketEvent?
                    │ none -> step() returns false (transport exhausted)
                    ▼ some
              Clk::advance(event.ts)
              Exec::on_market_event(event)
                    │
                    ▼
     RoundRobinPool<NumWorkers, ...>::run_round(EventContext{event, view})
     ┌─ NumWorkers threads, Strategies round-robin-assigned ─────┐
     │  StrategyTask<S>: strategy.on_event(event, view) -> Intent...│
     │  pushed to that strategy's SpscQueue<vector<Intent>>          │
     └─────────────────────────────────────────────────────────────┘
                    │ main drains queues 0..N-1 in that fixed order —
                    │ the wait and the ordering are the same operation
                    ▼
     for each Intent, in strategy-index order:
       Risk::check(intent, view) -> RiskDecision
              │ Approved/Resized -> Order
              ▼
       Exec::submit(order, Clk::now())
                    │
                    ▼
              Risk::on_tick(view) -> Order...  ──▶ Exec::submit(...)
                    │
                    ▼
        drain Exec::next_outcome() while some
              Fill   -> Portfolio::apply_fill
              Reject -> (no effect yet)
```

No concrete `Tx`/`Clk`/`Exec`/`Risk`/`Strategies` are named here — every
type is a template parameter constrained by its seam's concept. `apps/*`
remain the only place concrete adapters meet the concepts.

## Generic components

- **`Tx` (`transport::Transport`, D39)** — pulls the next `MarketEvent`, or
  signals exhaustion (`std::nullopt`). `LiveWebSocketSource`/
  `FileReplaySource` (`libs/data_source/source`), `InProcessTransport`, and
  `CombinedTransport` (`libs/data_source/transport`) all satisfy it;
  `Engine` depends on the concept only (`qp_transport`).
- **`Clk` (`qp::Clock`)** — `now()`/`advance(ts)`. `SimClock`
  (`libs/clock`) is the only adapter today; `WallClock` is deferred to the
  live milestone.
- **`Exec` (`execution::ExecutionGateway`)** — `submit(Order, Timestamp)` +
  ordered `next_outcome()` poll. `SimExecution<LastTradeMatcher>`
  (`libs/execution`) is the only adapter today.
- **`Risk` (`risk::RiskGate`)** — `check(Intent, StateView) -> RiskDecision`
  + `on_tick(StateView) -> vector<Order>`. No concrete implementation yet —
  a real kill-switch/drawdown-flatten policy is a later milestone.
- **`Strategies...` (`Strategy`, one or more)** — `on_event`/`on_timer` ->
  `vector<Intent>`. A closed, compile-time set, dispatched round-robin
  across `NumWorkers` threads by `RoundRobinPool` (`libs/core`, D33) — no
  concrete strategy exists yet.
- **`Portfolio`/`StateView`** (`libs/core`) — the shared feedback hub: one
  `Portfolio` per `Engine`, read via `StateView` by every strategy and by
  `Risk`, written only by `Engine::drain_outcomes()` on a `Fill`.

## High-level implementation

- `include/engine.hpp` — the `Engine` template: `step()` (one event),
  `run()` (drains the transport), `view()` (read-only `StateView` access
  for tests/callers). `StrategyTask<S>` adapts a `Strategy`'s 2-arg
  `on_event` to `RoundRobinPool`'s 1-arg `Result operator()(Context)` task
  shape.
- `tests/test_engine.cpp` — inline test doubles (`FakeTransport`,
  `SingleIntentStrategy`, `CountingStrategy`, `AlwaysApproveRiskGate`); five
  tests proving the loop composes end-to-end, including a 2-strategy,
  1-worker case.

## Goals

1. **Compiles against seams only, no concrete adapters.** `engine.hpp`
   names no venue, no real strategy, no real risk policy — every type is a
   template parameter constrained by its concept.
   - **Success metric:** `qp_engine_tests` links and passes using only
     test-double `Strategy`/`RiskGate` implementations and the existing
     `SimClock`/`SimExecution<LastTradeMatcher>` — nothing from `apps/*`.
2. **One full `step()` proven end-to-end.** A pulled event reaches a
   strategy, an approved `Intent` reaches `Exec::submit`, and the resulting
   `Fill` reaches `Portfolio` — the whole loop, not its pieces in
   isolation.
   - **Success metric:** `qp_engine_tests`' happy-path test asserts
     `Portfolio::position` after a single `step()` matches the `Intent` a
     test strategy emitted, driven by a real `Trade` event through the
     real `LastTradeMatcher`.
