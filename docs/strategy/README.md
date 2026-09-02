# strategy

The `Strategy` concept and the concrete strategies that implement it. A strategy consumes market events and emits `Intent`s.

## Diagram

```
MarketEvent / Timestamp
        │
        ▼
Strategy::on_event() / on_timer()      (strategy.hpp — the concept)
        │   concrete strategies plug in here
        ▼
   span<const Intent>
```

## Variations

- **`FundingCarryStrategy`** (`carry/`) — long spot, short perp, delta-neutral. Sizes and thresholds by `Config` to collect funding.

## Milestones

- [x] ~~`Strategy` concept~~
- [x] ~~Funding carry strategy~~
