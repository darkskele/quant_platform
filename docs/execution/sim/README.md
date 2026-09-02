# sim

`SimExecution`, the backtest `ExecutionGateway`. Wraps a `Matcher` with the accumulate/drain bookkeeping and records `Fill`s and `Reject`s.

## Diagram

```
Order / MarketEvent
        │
        ▼
   SimExecution                 (sim_execution.hpp)
        │   submit() forwards to
        ▼
   Matcher::try_fill()          (matcher/: the seam)
        │
        ▼
   fills() / rejects()
```

## Components

- `matcher/`: the `Matcher` concept, turns an `Order` into a `Fill` or `Reject`.

## Milestones

- [x] ~~`SimExecution` over a `Matcher`~~
