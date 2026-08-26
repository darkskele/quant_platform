# strategy — the `Strategy` concept + concrete strategies

`docs/strategy.md` (root) describes the strategy families and their
sequencing (D6: carry → stat-arb → factor → microstructure ML). This town
holds the `Strategy` concept (`strategy.hpp`) that every family implements;
one village plugs in below per family, starting with `carry/` (family 1).

## Diagram

```
MarketEvent, StateView(Portfolio)          (Engine, this town's caller)
        │
        ▼
Strategy::on_event() / ::on_timer()        (strategy.hpp — the concept;
        │                                   carry/ village plugs in here)
        ▼
    vector<Intent>
        │
        ▼
    RiskGate (libs/risk)
```

## Generic components

- **`Strategy` concept** (`strategy.hpp`) — the seam: `on_event(MarketEvent,
  StateView) -> vector<Intent>`, `on_timer(Timestamp, StateView) ->
  vector<Intent>`. Static dispatch (D27); depends only on
  `MarketEvent`/`StateView`/`Intent`, never a concrete adapter
  (`docs/architecture-principles.md`'s seam 4).

## High-level implementation

Town-level content is the concept; `carry/` is a flat leaf below it (single
implementation, no further nesting — same shape as `data_source/source`'s
`venue`/`resync`), described here rather than in a docs/ folder of its own.

- `strategy.hpp` — the `Strategy` concept.
- `tests/support/strategy_doubles.hpp` — `NoopStrategy`/`AlwaysIntentStrategy`,
  shared with `engine`'s tests/benchmarks.
- `carry/include/funding_carry_strategy.hpp` — `Config` + `FundingCarryStrategy`
  (D45): long spot + short perp, sized/thresholded by `Config`, satisfies
  `Strategy`.

## Goals

1. ~~**Build the `Strategy` concept**~~ — static-dispatch seam, `Engine`-consumed
   via `RoundRobinPool` (D27/D33).
2. ~~**Funding-rate/basis carry strategy**~~ (`docs/strategy.md`'s family 1,
   D6) — `carry/`: long spot + short perp, delta-neutral, collect funding
   (D45).
