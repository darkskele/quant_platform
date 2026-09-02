# sink

The `Sink` concept and the sinks that implement it. `record(MarketEvent)` never blocks; `false` means the event was dropped.

## Diagram

```
record(MarketEvent)      (sink.hpp — the concept)
        │   concrete sinks plug in here
        ▼
   bool (false = dropped, never blocks)
```

## Variations

- **`FanoutSink`** (`fanout/`) — redistributes recorded events to `NumConsumers` in-process readers via an SPMC queue.

## Milestones

- [x] ~~`Sink` concept~~
- [x] ~~`FanoutSink`~~
