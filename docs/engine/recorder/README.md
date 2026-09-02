# recorder

The `Recorder` concept and the recorders that implement it. A recorder is handed the timestamp and the portfolio once per processed event; it reads whatever it needs, or nothing.

## Diagram

```
Recorder::sample(ts, Portfolio)      (recorder.hpp: the concept)
        │   concrete recorders plug in here
        ▼
   recorded, or discarded
```

## Variations

- **`NullRecorder`** (`null/`): records nothing, the default, compiles away.
- **`EquitySeriesRecorder`** (`equity_series/`): pushes each sample to a lock-free queue that a drain thread empties into a growing series, off the record path.

## Milestones

- [x] ~~`Recorder` concept and no-op default~~
- [x] ~~equity series recorded off the record path~~
