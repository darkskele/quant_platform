# execution

The `ExecutionGateway` concept and the gateways that implement it. A gateway takes `Order`s and market events and produces `Fill`s and `Reject`s.

## Diagram

```
Order / MarketEvent
        │
        ▼
ExecutionGateway::submit() / on_market_event()   (execution_gateway.hpp — the concept)
        │   gateways plug in here
        ▼
span<const Fill> / span<const Reject>
```

## Variations

- **`SimExecution`** (`sim/`) — the backtest gateway. Wraps a `Matcher`; documented in its own README.

## Milestones

- [x] ~~`ExecutionGateway` concept~~
- [x] ~~Backtest gateway (`sim/`)~~
