# backtest

The backtest app. Composes source, sink, engine, execution, risk and strategy into a runnable backtest on the same code path as live. `BacktestBase` owns the lifecycle, runs the source pump and the engine on their own threads, and joins once the sources run dry. A variant supplies its sources, its sinks and its engine.

## Diagram

```
BacktestBase::run()  (two threads, joined)

  source pump   run_data_source(sources, sinks)
  thread              │  fills the sinks' fan-out queues
                      ▼
  engine        Engine::run()  (pulls via Transport)
  thread              │
                      ▼
                 Portfolio ─▶ results()
```

## Variations

- **`FundingCarryBacktest`** (`funding_carry/`): the carry strategy over a futures and a spot leg. Runtime `Config` for the carry and risk knobs, reports `Results`.
- **`PythonBinanceHistoricalBacktest`** (`python/`): Strategy and RiskGate are the python adapters, events come from the Binance historical source. Callbacks are set from the notebook via `set_on_event`/`set_on_timer`/`set_check`/`set_on_tick`, and `plan()` then `run()` drive it. Reports per-instrument fees, funding and basis alongside the equity series.

## Milestones

- [x] ~~Wire a strategy through a real `Engine` composition end to end~~
- [x] ~~Composition is runtime, driven by a `Subscription`~~
- [x] ~~A real backtest run against real exchange data~~
- [ ] Wire `FundingCarryBacktest` onto a live source
