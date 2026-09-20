# transport decisions

1. `Transport` is the pull seam: `next()` yields one `EngineInput` or `nullopt`, `flush()` switches to draining without waiting on every leg.
2. The timestamp is on every pull, not only on events, so it is replay time in backtest and receive time live without anything downstream branching.
3. A pull with no event is a timer tick. The transport owns the timer, since only it knows where the boundary falls between two events.
4. A timer boundary at or before the next event fires first, at its own timestamp, without consuming the event.
5. `BacktestInProcessTransport` merges a fixed set of queues in ascending `ts` order with a one-slot lookahead per queue.
6. It is the only `Transport` merge implementation; a round-robin merge over heterogeneous transports was removed as no caller used it.
