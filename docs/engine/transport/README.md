# transport

The `Transport` concept and the transports that implement it. A transport yields one `MarketEvent` at a time or `nullopt`. `flush()` switches `next()` from waiting on every leg to draining what is buffered.

## Diagram

```
Transport::next() / flush()      (transport.hpp: the concept)
        │   concrete transports plug in here
        ▼
   optional<MarketEvent>
```

## Variations

- **`BacktestInProcessTransport`** (`backtest_in_process/`): merges a fixed set of queues in ascending `ts` order, one lookahead slot per queue.

## Milestones

- [x] ~~`Transport` concept~~
- [x] ~~`BacktestInProcessTransport`~~
