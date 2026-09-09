# cost_aware

`CostAwareMatcher`, a matcher variation that composes a `CostModel`. Fills at the last-seen price adjusted by the cost model, rejects when the cost model has no data for the order.

## Diagram

```
Order / MarketEvent
        │
        ▼
CostAwareMatcher::try_fill() / on_market_event()
        │   delegates fill/fee pricing to
        ▼
CostModel::price()                 (cost_model/: the seam)
        │
        ▼
variant<Fill, Reject>
```

## Components

- `cost_model/`. Prices a fill and its fee for an `Order` at a reference price and time.

## Milestones

- [x] ~~`CostAwareMatcher` composing a `CostModel`~~
- [x] ~~Reject on cost-model miss (`RejectReason::NoCostAvailable`)~~
