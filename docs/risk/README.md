# risk

The `RiskGate` concept and the concrete gates that implement it. A gate turns an `Intent` into a sized `Order` or rejects it, and runs autonomous risk checks each tick.

## Diagram

```
Intent
   │
   ▼
RiskGate::check() / on_tick()      (risk_gate.hpp — the concept)
   │   concrete gates plug in here
   ▼
RiskDecision / span<const Order>
```

## Variations

- **`BasicRiskGate`** (`basic/`) — per-(symbol, venue) exposure cap in `check()`, equity-drawdown kill switch in `on_tick()`.

## Milestones

- [x] ~~`RiskGate` concept + `RiskDecision`~~
- [x] ~~Concrete gate: exposure cap + drawdown kill switch~~
