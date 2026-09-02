# engine

The trader composition root. Composes a `Transport`, `Clock`, `ExecutionGateway`, `RiskGate`, `Strategy`, a shared `Portfolio`, and a `Recorder` into one event loop. Every collaborator is a template parameter constrained by its seam concept.

## Diagram

```
Transport::next() ──▶ MarketEvent?
        │ none → step() returns false
        ▼ some
Clock::advance(ts)
Exec::on_market_event(event)
Portfolio::apply_funding / apply_mark_price
        │
        ▼
Strategy::on_event(event) ──▶ Intents
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
Recorder::sample(Clock::now(), Portfolio)   (per step; no-op by default)
```

## Components

- `transport/`: the `Transport` concept, the event seam the loop pulls from.
- `recorder/`: the `Recorder` concept, the per-step observation seam the loop feeds equity to.

## Milestones

- [x] ~~Compiles against seams only, no concrete adapters~~
- [x] ~~One full `step()` end-to-end: event → strategy → risk → exec → fill → `Portfolio`~~
