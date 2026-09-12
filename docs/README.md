# quant-platform

A modular C++ trading platform where **backtest and live run the same code**. A strategy proven in research behaves identically in production. The seams are venue- and asset-agnostic: Binance USD-M perpetuals and medium-frequency holding periods are the current target, not a constraint. The edge is structural and statistical. The platform is the durable asset, correct and deterministic and trustworthy; strategies are experiments run on top of it.

## How it fits together

An event flows one way through composed seams. Each seam is a concept with swappable implementations, selected at compile time.

```
Source ─▶ Sink ─▶ Transport ─▶ Engine.step()
                                   │
                                   ├─ Strategy    event  ─▶ Intent
                                   ├─ Risk        Intent ─▶ Order | reject
                                   ├─ Execution   Order  ─▶ Fill
                                   └─ Portfolio   Fill applied
```

- **[data_source](data_source/README.md)**: pulls venue events (`Source`), fans them out (`Sink`), pairs and stamps them (`run_data_source`).
- **[clock](clock/README.md)**: the injected time seam (`Clock`). Strategy and risk never read the system clock.
- **[engine](engine/README.md)**: the composition root. `step()` pulls from a `Transport`, runs strategy → risk → execution, updates the `Portfolio`.
- **[strategy](strategy/README.md)**: turns events into `Intent`s (`Strategy`).
- **[risk](risk/README.md)**: turns `Intent`s into sized `Order`s or rejects, with autonomous kill-switch authority (`RiskGate`).
- **[execution](execution/README.md)**: turns `Order`s into `Fill`s (`ExecutionGateway`, `Matcher`).
- **[apps/backtest](apps/backtest/README.md)**: composes all of it into a runnable backtest on the live code path.
- **core**: shared primitives. `MarketEvent`/`Intent`/`Order`/`Fill`, `Portfolio`, lock-free queues, `ViewablePool`.

Each module's README covers its own concept, variations, and milestones. Each `DECISIONS.md` records that module's choices.

## Design philosophy

- **One code path.** Backtest and live differ only in compile-time policy types (`Source`/`Clock`/`ExecutionGateway`/`Sink`). Never fork logic on a flag.
- **Determinism.** Injected `Clock`, no hidden state. A replay always produces the same decisions. That is what makes a backtest trustworthy for live.
- **Plug and play.** Every seam is a concept. Implementations swap without touching consumers, and static dispatch keeps it zero-cost.
- **Performance where the software controls it.** Lock-free data paths, compile-time dispatch, cache-aware layout. Not because speed is the edge, but because self-inflicted overhead is cost regardless.
- **Honest costs.** Fees, funding, slippage, partials in sim execution. An optimistic fill is how a backtest lies.

## Repo layout

`docs/` mirrors `include/`. `cmake/<module>` derives its `include`/`tests`/`benchmarks` dirs by path. One concept per directory; variations nest below.

To add a component: create its dir under `include/`, mirror it in `cmake/`, `tests/`, `benchmarks/`, and add a `README.md` + `DECISIONS.md` under `docs/`.

## Milestones

Platform:

- [x] ~~Funding-carry strategy~~
- [x] ~~First backtest engine, full composition runs~~
- [ ] Research infra: pybind, notebooks, tune the funding-carry config
- [ ] More sophisticated risk gate
- [ ] More sophisticated sim execution
- [ ] Analytics and observability: metrics, logs, equity/PnL series (for live, not backtest)
- [ ] Live execution and data via testnet (same code, testnet endpoints)
- [ ] Remote data store: partitioned parquet, catalog, one store for research and the engine

Strategies, in order of increasing sophistication and data appetite:

- [x] ~~Funding / basis carry~~
- [ ] Time-series trend
- [ ] Cross-sectional statistical arbitrage: residual reversal, momentum, funding
- [ ] Open interest and positioning signals, regime gating
- [ ] Intraday flow and reversal on depth and trades
- [ ] Maker execution and market making on top-of-book
- [ ] Order-book / ML microstructure, liquidation and options on paid L2
