# matcher

The `Matcher` concept and the concrete matchers. A matcher turns an `Order` into a `Fill` or `Reject` against market state.

## Diagram

```
Order / MarketEvent
        │
        ▼
Matcher::try_fill() / on_market_event()   (matcher.hpp — the concept)
        │   concrete matchers plug in here
        ▼
   variant<Fill, Reject>
```

## Variations

- **`LastTradeMatcher`** (`last_trade/`) — fills a market order fully and instantly at the last-seen trade price.

## Milestones

- [x] ~~`Matcher` concept~~
- [x] ~~`LastTradeMatcher`~~
