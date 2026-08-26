# risk status

- ~~G1 — Build the `RiskGate` concept + `RiskDecision`~~
- ~~G2 — A concrete `RiskGate`: exposure cap + drawdown kill switch~~

## Last proof

**`BasicRiskGate` built** (G2, D46) — `check()`: computes the delta against
`StateView::position()` (not the raw target), clamps to `max_position_qty`
(`Resized` when clamped), sizes the `Order` from the clamped delta.
`on_tick()`: tracks peak `StateView::equity()` (D46's new `Portfolio`
method), flattens every (symbol, venue) it has seen via `check()` once
equity has declined `max_drawdown` (an absolute amount, not a percentage —
`Portfolio` has no allocated-starting-capital concept) from that peak.
Trips once, permanently — no auto-resume. Not yet built/run this pass (see
CLAUDE.md's build workflow). `qp_risk_tests` (`test_basic_risk_gate.cpp`):
flat/delta sizing, clamp+`Resized`, no-drawdown no-op, trip+flatten,
stays-tripped, rejects-everything-once-tripped. `qp_risk_bench` added:
`check()` approve/resize paths, `on_tick()`'s no-drawdown fast path (the
one that actually accumulates cost — it runs on every `Engine::step()`,
not just when a trip fires).

**`RiskGate` concept built** (G1, D27/D28) — `check`/`on_tick` signatures,
static dispatch. `qp_risk_tests` passing (`AlwaysApproveRiskGate`/
`AlwaysRejectRiskGate` satisfy the concept).
