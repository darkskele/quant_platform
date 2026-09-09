# matcher

The `Matcher` concept and the concrete matchers. A matcher turns an `Order` into a `Fill` or `Reject` against market state.

## Diagram

```
Order / MarketEvent
        │
        ▼
Matcher::try_fill() / on_market_event()   (matcher.hpp: the concept)
        │   concrete matchers plug in here
        ▼
   variant<Fill, Reject>
```

## Variations

- **`LastTradeMatcher`** (`last_trade/`). Fills a market order fully and instantly at the last-seen trade price.
- **`CostAwareMatcher`** (`cost_aware/`). Fills at the last-seen price adjusted by a composed `CostModel` sub-seam.

## Milestones

- [x] ~~`Matcher` concept~~
- [x] ~~`LastTradeMatcher`~~
- [x] ~~Honest cost matcher~~
- [ ] Book-walking `Matcher` variation
    - fills against L2 book depth from the historical feed
