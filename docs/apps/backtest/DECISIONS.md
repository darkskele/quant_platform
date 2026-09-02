# backtest decisions

1. Backtest and live share one code path; only the compile-time component types differ.
2. `BacktestBase` runs the source pump and the engine on their own threads, joining once the sources run dry.
3. Everything but the swept knobs is compile-time: symbol, dataset, day range, and component types are macros with static-asserts; only the `carry`/`risk` `Config` is runtime — the knobs a Python optimizer sets between runs.
4. No CLI entry point; a run is driven by constructing a backtest and calling `run()` (the pybind path).
5. `FundingCarryBacktest` wires futures + spot CSV legs; venue indices match the source/sink pairing order, a fact about how the streams merge, not a config knob.
6. `run()` returns a `Results` struct (final cash/equity/positions), not the internal `Portfolio`.
