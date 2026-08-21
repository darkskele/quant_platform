# Architecture & design principles

## Frame: ports and adapters (hexagonal)

The **core** — strategy logic, risk logic, book-building — is pure and testable
and depends only on abstract contracts. The **adapters** — where data comes
from, where orders go — are swappable, at the edges. The whole "backtest ==
live" guarantee is that *the core can't tell which adapters it's plugged into.*

## The metamodel is not a one-way pipe

`data → signal → risk → execution → live` is the dominant **decision** flow, but:

- **Fills flow back.** Execution → position/PnL, which risk *and* strategies
  read. Modeled as a shared authoritative **state** that execution writes and
  signal/risk read — feedback is a typed event (`Fill`, `PositionUpdate`), never
  a hidden back-pointer.
- **Risk is a gate, not a pass-through** — can reject/resize, and acts on its own
  (kill-switch flattens with no signal upstream).
- **Exchange + clock are side-inputs.** Acks/fills/funding arrive async (gateway
  is source *and* sink); many strategies fire on a **timer**, not on market data.

So: an **event-driven core** with typed events + a small shared state, not a
Unix pipe.

## What the metamodel governs

Component boundaries, thread/process topology, IPC/messaging, and the event
schema — **not** strategy math. At MFT a **single process, a few threads**
(feed / strategy / recorder) wired by SPSC queues is sufficient; multi-process
shared-memory IPC is an HFT latency pattern we don't need. Clean seams keep the
door open to cut a process boundary later for *operational* reasons (restart the
trader without dropping the feed), not speed.

**"Not competing on network/hardware latency" ≠ "computational efficiency
doesn't matter."** D1 rules out colo/kernel-bypass/FPGAs — racing the wire
with hardware we can't buy. It does *not* rule out chasing every nanosecond
the software controls: SIMD, cache-conscious layouts, lock-free structures
(the SPSC queue, direct-array-indexed gap detection, simdjson's SIMD
parsing), and — per D27 — static dispatch on every seam, strategy/risk
included, not just the ones that happen to be I/O-adjacent. Phase 4's
order-book feature engine is the natural home for more of it — e.g. SIMD
level-aggregation — since that's genuinely hot, high-volume, on-box compute,
not a race against another firm's network path.

## The seams

1. **Event schema — the lingua franca.** `MarketEvent` (book update, trade,
   funding), `Order`, `Fill`, `Ack/Reject`, `PositionUpdate`. Plain data, no
   behavior, no venue-specifics. Get this right and the rest composes.

2. **`Transport` (concept, compile-time)** — `next() -> optional<MarketEvent>`.
   Adapters: `LiveWebSocketSource`, `FileReplaySource`, an in-process ring
   reader. Named `Transport`, not `Source` — `source`/`sink` already name
   the two `data_source` cities, and an adapter reading off a `Sink`'s
   fan-out ring is not itself a "source." Book-building lives *behind* this
   seam so every adapter produces identical events.

3. **`Clock` / `TimeSource` (concept, compile-time) — the determinism landmine.**
   `now() -> Timestamp`. `WallClock` (live) / `SimClock` (backtest, driven by
   event timestamps). **Strategy code must NEVER call the system clock directly**
   — that silently breaks backtest determinism.

4. **`Strategy` (concept, compile-time)** — `on_event(MarketEvent, StateView) ->
   vector<Intent>`, `on_timer(...)`. Emits **intent** ("be +2 BTC"), not venue
   calls — stays execution-agnostic and unit-testable. A closed,
   compile-time set of strategies (`std::tuple<Strategies...>`, fold-dispatched)
   runs inside one `Engine` — no vtable, no per-strategy heap allocation (D27).

5. **`RiskGate` (concept, compile-time)** — `check(Intent, StateView) -> RiskDecision`
   (approve/resize/reject → concrete `Order`s) + `on_tick(StateView) ->
   vector<Order>` (autonomous kill-switch / drawdown flatten). Where intent
   becomes sized orders. Has its own authority.

6. **`Portfolio` / state — the feedback hub.** Positions, open orders, PnL.
   Exposes read-only `StateView` to strategy/risk; write path fed by `Fill`
   events. The read/write split keeps the backward flow honest.

7. **`ExecutionGateway` (concept, compile-time) — the prod/test switch itself.**
   `submit(Order)`; fills come back as *events*. `SimExecution` matches against
   the replayed book with the honest cost model (fees/slippage/partials/funding)
   — backtest realism lives or dies here. `LiveExecution` talks to Binance.

8. **`Recorder` / sink (concept, compile-time)** — tee consumer on its own thread
   behind an SPSC queue, writing compressed binary partitions. No-op in pure
   backtest.

## Compile-time vs virtual — the rule

**Static dispatch everywhere the software controls the cost** — transport,
clock, execution, recorder, strategy, risk all resolve at compile time
(concepts, not virtual bases; `std::tuple<Strategies...>` + a fold
expression for the closed, heterogeneous strategy set). We're not racing
colo/kernel-bypass/FPGA hardware (D1) — that's a resource constraint, not
permission to waste the latency we do control. A vtable indirection or a
heap allocation is a self-inflicted cost, same category as an unnecessary
copy or a missed cache line; "the budget is huge so it doesn't matter" was
D4's mistake, superseded by D27. Virtual dispatch is reserved for a
genuinely rare, config-selected choice with no hot-path exposure — none of
the current seams qualify; revisit if one actually appears.

## Composition — share the SOURCE, not the engine

The property worth guaranteeing is that the book you **record** (and later
backtest against) is built by the *same code* that builds the book the live
strategy sees. That code lives in the **source** (`LiveWebSocketSource`), not in
the engine. So the thing that's shared is the source; the engine is only for the
trader.

**The trader** (live/backtest) is an Engine parameterized on compile-time
policies:

```cpp
template<Transport Tx, Clock Clk, ExecutionGateway Exec, RiskGate Risk,
         std::size_t NumWorkers, Strategy... Strategies>
class Engine {
    Tx transport_; Clk clock_; Exec exec_; Risk risk_;
    RoundRobinPool<NumWorkers, ..., Strategies...> pool_;  // strategies, round-robin (D33)
    Portfolio state_;
    // loop: pull event -> pool_.run_round(strategies) -> risk/exec, in strategy order -> fills -> state
};

// live.cpp     Engine<LiveWebSocketSource, WallClock, LiveExecution, ...>
// backtest.cpp Engine<FileReplaySource,    SimClock,  SimExecution,  ...>
```

No `Sink` param — recording is orthogonal to the decision loop, not
something the backtest side needs to satisfy with a no-op. Live recording,
if wanted alongside trading, wraps the `Transport` (a tee) or runs the
collector as its own process; `Engine` itself never touches a `Sink`.

**The collector** is NOT an Engine — it's a thin source→recorder loop. Bundling
`ExecutionGateway` into the Engine as a policy means an engine-shaped collector
would need a fake `NullExecution` it never uses — a smell. Instead:

```cpp
// collector.cpp
LiveWebSocketSource source{...};
FileRecorder        recorder{...};   // on its own thread behind an SPSC queue
while (auto ev = source.next()) recorder_queue.push(*ev);
```

The recorder taps the **normalized event stream emitted by the source**, so the
engine is irrelevant to recording. Keep the recorder on its own thread behind an
SPSC queue even here: if a disk write blocks while you read the socket on the
same thread, the receive buffer backs up, messages drop, and the book desyncs —
silently corrupting the recording.

Dependency direction is the discipline: strategy/risk depend only on
`MarketEvent` / `StateView` / `Intent`, never on concrete adapters. Concrete
types are named only in the `apps/*` mains.

## Stated principles

- **One code path.** Backtest and live share the core; only source/clock/
  execution/sink policies differ. Deterministic replay is sacred.
- **Event-driven core, shared authoritative state.** Feedback is a typed event.
- **Static where frequent, virtual where configurable.**
- **Seams first, generality later.** Define interfaces cleanly, build concretely,
  let abstraction earn its place on the third implementation (no plugin
  framework before a plugin).
- **Collect from day one, losslessly** (binary, compressed, partitioned).
- **Honest costs are first-class** in the backtester.
- **Risk has its own authority** (kill-switch).
- **Uptime and correctness beat cleverness.**

## The two easy-to-botch bits

1. **`Clock`** — never touch the system clock in strategy code.
2. **`SimExecution` fill model** — an optimistic fill sim is how honest-looking
   backtests lie.
