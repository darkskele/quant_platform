# risk

The `RiskGate` concept and the concrete gates that implement it. A gate turns an `Intent` into a sized `Order` or rejects it, and runs autonomous risk checks each tick.

## Diagram

```
Intent
   │
   ▼
RiskGate::check() / on_tick()      (risk_gate.hpp: the concept)
   │   concrete gates plug in here
   ▼
RiskDecision / span<const Order>
```

## Variations

- **`BasicRiskGate`** (`basic/`): per-instrument exposure cap in `check()`, equity-drawdown kill switch in `on_tick()`.
- **`PythonRiskGate`** (`python/`): forwards `check`/`on_tick` to `py::object` callables so the gate body can live in a notebook.

## Milestones

- [x] ~~`RiskGate` concept + `RiskDecision`~~
- [x] ~~Concrete gate: exposure cap + drawdown kill switch~~
- [x] ~~Python adapter~~
- [ ] Contract preconditions on gate inputs, finite targets and known instruments, enforced in debug and ignored in release (C++26, GCC 16)
    - the kill switch stays plain code, never a contract
