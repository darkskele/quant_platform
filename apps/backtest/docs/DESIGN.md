# backtest

## Diagram

```
argv
 │
 ▼
parse_args() ──▶ Config
                   │  data_dir, symbol, first_day/last_day, carry overrides, risk overrides
                   ▼
                  run(Config)
                   │
        ┌──────────┴──────────────────────────────────────┐
        │ resolve_symbol_id(data_dir, symbol) — cross-checks   │
        │ futures/spot symbols.manifest agree (D48)            │
        └──────────┬──────────────────────────────────────┘
                   │
        ┌──────────┴──────────────────┐
        ▼                              ▼
  FileReplaySource               FileReplaySource
  (data_dir/futures)             (data_dir/spot)
  venue=0 (D48)                  venue=1 (D48)
  (libs/data_source/source)      (libs/data_source/source)
        │                              │
        └──────────────┬───────────────┘
                        ▼
              CombinedTransport<FRS, FRS>
              (libs/data_source/transport)
                        │
                        ▼
   Engine<Tx, SimClock, SimExecution<LastTradeMatcher>,
          BasicRiskGate, 1, FundingCarryStrategy>
   (libs/engine)
                        │
                        ▼
                    Results
        (final_cash/equity/spot_position/futures_position)
```

`run()` owns the whole replay — single-threaded end to end (`Engine::run()`
drives `step()` to completion; `FileReplaySource` is itself synchronous, no
reader thread), unlike the collector, which has real I/O to protect with
its own threading.

## Generic components

- **`Config`** (`backtest.hpp`) — `data_dir` (root; expects `futures/`/
  `spot/`, the collector's own D41 layout), `symbol` (the one instrument
  this run trades), `first_day`/`last_day` (inclusive UTC range),
  `carry`/`risk` sub-configs (`strategy::carry::Config`/
  `risk::BasicRiskGateConfig`) for the runtime-swept thresholds — `carry`'s
  `symbol`/`spot_venue`/`futures_venue` are filled in by `run()`, not meant
  to be set by a caller.
- **`FileReplaySource`** (`libs/data_source/source`) — the backtest
  `Source`/`Transport`: one instance per leg, filtered to `config.symbol`.
- **`CombinedTransport<FileReplaySource, FileReplaySource>`**
  (`libs/data_source/transport`) — merges both legs into the single `Tx`
  `Engine` consumes.
- **`SimClock`**/**`SimExecution<LastTradeMatcher>`** — the backtest
  `Clock`/`ExecutionGateway` (D22/D25); no live variant swapped in here.
- **`BasicRiskGate`** (`libs/risk`, D46/D47) — the exposure-cap +
  drawdown-kill-switch `RiskGate`.
- **`FundingCarryStrategy`** (`libs/strategy/carry`, D45) — the one
  `Strategy` this app wires; `Engine<..., 1, FundingCarryStrategy>` since a
  single-strategy run needs no more than one `RoundRobinPool` worker.
- **`Results`** (`backtest.hpp`) — final cash/equity/position, not the raw
  `Portfolio` (an internal representation, not meant for external
  consumption). Enough to prove the wiring and give a test something to
  assert against; a real analytics layer (Sharpe, drawdown curve, ...) is
  later, deliberately deferred work.

## High-level implementation

- `backtest.hpp`/`.cpp` — `Config`, `Results`, `parse_args`, `run`.
- `main.cpp` — thin entrypoint: parse, run, print `Results`, propagate a
  thrown `std::runtime_error` (D48's manifest cross-check) as a non-zero
  exit rather than a crash.
- `tests/test_backtest_integration.cpp` — end-to-end: synthetic
  `FileRecorder`-written futures+spot data (not a real collector run) fed
  through the full composition, asserting on `Results`.

## Goals

1. **Wire `FundingCarryStrategy` through a real `Engine` composition,
   end to end.**
   - **Success metric:** `qp_backtest_integration_tests` proves a
     `Funding` event clearing the entry threshold produces the expected
     delta-neutral spot/futures position and fill economics (fees, no
     phantom PnL) — not just that the composition compiles.
2. **A real backtest run against real recorded data** — needs the
   collector to have actually run and accumulated `futures/`/`spot/`
   history first; not provable by this app's own tests (which use
   synthetic data, matching `apps/collector`'s own "unit tests use
   synthetic/captured fixtures, real end-to-end proof is a later
   milestone" pattern).
   - **Success metric:** N/A today — no accumulated recording exists yet.
