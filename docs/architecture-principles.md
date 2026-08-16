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
doesn't matter."** D1 rules out colo/kernel-bypass/FPGAs — racing the wire.
It doesn't rule out SIMD, cache-conscious layouts, or lock-free structures
where the codebase already benefits from them without hardware investment
(the SPSC queue, direct-array-indexed gap detection, simdjson's SIMD
parsing are already this discipline, just not previously named). Phase 4's
order-book feature engine is the natural home for more of it — e.g. SIMD
level-aggregation — since that's genuinely hot, high-volume, on-box compute,
not a race against another firm's network path.

## The seams

1. **Event schema — the lingua franca.** `MarketEvent` (book update, trade,
   funding), `Order`, `Fill`, `Ack/Reject`, `PositionUpdate`. Plain data, no
   behavior, no venue-specifics. Get this right and the rest composes.

2. **`MarketDataSource` (concept, compile-time)** — `next() -> optional<MarketEvent>`.
   Adapters: `LiveWebSocketSource`, `FileReplaySource`. Book-building lives
   *behind* this seam so both produce identical events.

3. **`Clock` / `TimeSource` (concept, compile-time) — the determinism landmine.**
   `now() -> Timestamp`. `WallClock` (live) / `SimClock` (backtest, driven by
   event timestamps). **Strategy code must NEVER call the system clock directly**
   — that silently breaks backtest determinism.

4. **`Strategy` (virtual, runtime)** — `on_event(MarketEvent, StateView) ->
   vector<Intent>`, `on_timer(...)`. Emits **intent** ("be +2 BTC"), not venue
   calls — stays execution-agnostic and unit-testable. Virtual because chosen by
   config, held in collections, swappable; vtable cost is irrelevant at this
   frequency.

5. **`RiskGate` (virtual, runtime)** — `check(Intent, StateView) -> RiskDecision`
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

**Static dispatch where events are frequent and the swap is build-fixed**
(source, clock, execution, recorder). **Virtual where configuration is runtime**
(which strategy, which risk config). The choice is a function of *how often you
cross the seam relative to your latency budget* — at MFT the strategy seam is
crossed rarely against a huge budget, so virtual is free; at HFT it'd be
templated (or `std::variant`+`visit`) so the hot path inlines.

## Composition — share the SOURCE, not the engine

The property worth guaranteeing is that the book you **record** (and later
backtest against) is built by the *same code* that builds the book the live
strategy sees. That code lives in the **source** (`LiveWebSocketSource`), not in
the engine. So the thing that's shared is the source; the engine is only for the
trader.

**The trader** (live/backtest) is an Engine parameterized on compile-time
policies:

```cpp
template<MarketDataSource Src, Clock Clk, ExecutionGateway Exec, Sink Rec>
class Engine {
    Src source_; Clk clock_; Exec exec_; Rec recorder_;   // recorder tee optional
    std::vector<std::unique_ptr<Strategy>> strategies_;
    std::unique_ptr<RiskGate> risk_;
    Portfolio state_;
    // loop: pull event -> (recorder tee) -> strategies -> risk -> exec -> fills -> state
};

// live.cpp     Engine<LiveWebSocketSource, WallClock, LiveExecution, FileRecorder>
// backtest.cpp Engine<FileReplaySource,    SimClock,  SimExecution,  NullSink>
```

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
