# risk decisions

1. `RiskGate` is a static-dispatch concept: `check(intent)` returns a `RiskDecision`, `on_tick()` returns orders.
2. `RiskDecision` is a tag (`Approved`/`Resized`/`Rejected`) plus an optional `Order`; the tag says why, not what shape.
3. `BasicRiskGate` is the first variation: per-instrument exposure cap in `check()`, equity-drawdown kill switch in `on_tick()`.
4. Drawdown is an absolute `Notional` decline from peak, not a percentage, since `Portfolio` has no starting-capital concept to divide by.
5. The kill switch trips once and stays tripped; resuming is an operator decision.
6. The instruments to flatten on a trip are named in the config, so a trip never has to discover what the book holds.
7. `PythonRiskGate` acquires the GIL per call and forwards to two `py::object` callables. Single-threaded backtest scope.
8. A `py::none` callback slot means a no-op; `None` from `check` is `Rejected`, `None` from `on_tick` is an empty order iterable.
