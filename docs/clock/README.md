# clock

The `Clock` concept and the clocks that implement it. `now()` reads the current time, `advance(ts)` moves it forward.

## Diagram

```
advance(ts) ──▶ now()      (clock.hpp: the concept)
        │   concrete clocks plug in here
        ▼
     Timestamp
```

## Variations

- **`SimClock`** (`sim_clock.hpp`): backtest clock. `now()` returns the last `advance(ts)`, driven by event timestamps rather than the wall clock.

## Milestones

- [x] ~~`Clock` concept~~
- [x] ~~`SimClock`~~
