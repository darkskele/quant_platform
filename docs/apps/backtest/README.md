# backtest

The backtest app. Composes source, sink, engine, execution, risk, and strategy into a runnable backtest on the same code path as live. `config/` selects the concrete component types at compile time; `BacktestBase` runs the source pump and the engine on their own threads.

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

- **`FundingCarryBacktest`** (`funding_carry/`): futures + spot CSV legs merged in `ts` order. Runtime-swept `Config` (carry + risk knobs), reports `Results`.

## Milestones

- [x] ~~Wire a strategy through a real `Engine` composition end to end~~
- [ ] A real backtest run against accumulated recorded data
