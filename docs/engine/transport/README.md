# transport

The `Transport` concept and the transports that implement it. A transport yields one `EngineInput`, a timestamp with an optional `MarketEvent`, or `nullopt` when nothing is ready. `flush()` switches `next()` from waiting on every leg to draining what is buffered.

## Diagram

```
Transport::next() / flush()      (transport.hpp: the concept)
        │   concrete transports plug in here
        ▼
   optional<EngineInput>   ts, plus an event or a timer tick
```

## Variations

- **`BacktestInProcessTransport`** (`backtest_in_process/`). Merges a fixed set of queues in ascending `ts` order, one lookahead slot per queue, and interleaves timer ticks on a fixed period into the same order.

## Milestones

- [x] ~~`Transport` concept~~
- [x] ~~`BacktestInProcessTransport`~~
- [x] ~~Timer ticks on the replay time line~~
