# engine

The trader composition root. Composes a `Transport`, `ExecutionGateway`, `RiskGate`, `Strategy`, a shared `Portfolio`, and a `Recorder` into one event loop. Every collaborator is a template parameter constrained by its seam concept.

## Diagram

```
Transport::next() ──▶ EngineInput { ts, event? }
        │ none → step() returns false
        ▼
   event present                          event absent
Exec::on_market_event(event)              timer tick at ts
Portfolio::apply_funding / apply_mark_price
Strategy::on_event(event)                 Strategy::on_timer(ts)
        │   each Intent
        ▼
Risk::check(intent) ──▶ approved Order ──▶ Exec::submit()
        │
        ▼
Risk::on_tick() ──▶ Orders ──▶ Exec::submit()
        │
        ▼
drain Exec::fills() ──▶ Portfolio::apply_fill
        │
        ▼
Recorder::sample(ts, Portfolio)   (per step; no-op by default)
```

## Components

- `transport/`. The `Transport` concept, the event and time seam the loop pulls from.
- `recorder/`. The `Recorder` concept, the per-step observation seam the loop feeds equity to.

## Milestones

- [x] ~~Compiles against seams only, no concrete adapters~~
- [x] ~~One full `step()` end-to-end: event → strategy → risk → exec → fill → `Portfolio`~~
- [x] ~~Timer ticks interleaved with events on one time line~~
