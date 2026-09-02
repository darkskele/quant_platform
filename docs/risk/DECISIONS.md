# risk decisions

1. `RiskGate` is a static-dispatch concept: `check(intent)` returns a `RiskDecision`, `on_tick()` returns orders.
2. `RiskDecision` is a tag (`Approved`/`Resized`/`Rejected`) plus an optional `Order`; the tag says why, not what shape.
3. `BasicRiskGate` is the first variation: per-(symbol, venue) exposure cap in `check()`, equity-drawdown kill switch in `on_tick()`.
4. Drawdown is an absolute `Notional` decline from peak, not a percentage, since `Portfolio` has no starting-capital concept to divide by.
5. The kill switch trips once and stays tripped; resuming is an operator decision.
6. Known (symbol, venue) pairs are tracked in a flat bool array indexed by dense ids, not a set.
