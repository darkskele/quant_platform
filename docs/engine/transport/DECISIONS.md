# transport decisions

1. `Transport` is the pull seam: `next()` yields one `MarketEvent` or `nullopt`, `flush()` switches to draining without waiting on every leg.
2. `BacktestInProcessTransport` merges a fixed set of queues in ascending `ts` order with a one-slot lookahead per queue.
3. It is the only `Transport` merge implementation; a round-robin merge over heterogeneous transports was removed as no caller used it.
