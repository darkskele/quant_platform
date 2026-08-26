# risk — the `RiskGate` concept + concrete gates

Where `Intent` becomes sized `Order`(s) — has its own authority, not a
pass-through (`docs/architecture-principles.md` seam 5). Autonomous
authority (CLAUDE.md: kill-switch/drawdown flatten) lives in `on_tick()`,
called every `Engine::step()` with no `Intent` input.

## Diagram

```
Intent, StateView(Portfolio)               (Engine, this lib's caller)
        │
        ▼
RiskGate::check() / ::on_tick()            (risk_gate.hpp — the concept;
        │                                   basic_risk_gate.hpp plugs in here)
        ▼
    RiskDecision / vector<Order>
        │
        ▼
    ExecutionGateway (libs/execution)
```

## Generic components

- **`RiskGate` concept** (`risk_gate.hpp`) — the seam: `check(Intent,
  StateView) -> RiskDecision`, `on_tick(StateView) -> vector<Order>`.
  Static dispatch (D27).
- **`RiskDecision`/`RiskOutcome`** (`risk_gate.hpp`) — a tag
  (`Approved`/`Resized`/`Rejected`) + `optional<Order>` (D28): `Approved`/
  `Resized` share one payload shape, only `Rejected` differs, so a tag says
  *why*, not *what shape*.
- **`BasicRiskGate`** (`basic_risk_gate.hpp`, D46) — the first concrete
  gate: a per-(symbol, venue) exposure cap in `check()`, an equity-drawdown
  kill switch in `on_tick()`.

## High-level implementation

- `risk_gate.hpp` — the `RiskGate` concept + `RiskDecision`/`RiskOutcome`.
- `basic_risk_gate.hpp` — `BasicRiskGateConfig` + `BasicRiskGate`.
- `tests/support/risk_gate_doubles.hpp` — `AlwaysApproveRiskGate`/
  `AlwaysRejectRiskGate`, shared with `engine`'s tests/benchmarks.

## Goals

1. ~~**Build the `RiskGate` concept + `RiskDecision`**~~ — static-dispatch
   seam, `Engine`-consumed (D27/D28).
2. ~~**A concrete `RiskGate`: exposure cap + drawdown kill switch**~~
   (D46) — `BasicRiskGate`: `check()` clamps `Intent::target_position` to a
   configured `max_position_qty`; `on_tick()` flattens every known
   (symbol, venue) once equity has declined `max_drawdown` from its peak.
