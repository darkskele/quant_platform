# core decisions

1. Timestamps are `int64` nanoseconds. Prices, quantities and notionals are doubles.
2. A market event is one `EventBase` plus a payload variant. The base carries kind, instrument and stamp for every payload alike.
3. A bar's open time is its identity. The event is stamped at its close.
4. Book depth bands sit behind a shared pointer, so the payload variant stays as narrow as the small events.
5. An `Intent` carries a target position, not a delta.
6. Instruments are small integer (exchange, market, symbol) slots resolved once from a `Subscription`. Per-instrument state is a flat array indexed by them.
7. Every queue is bounded and a power of two in capacity. A full push returns false and the caller decides.
8. Cursors and ready flags sit on their own cache line.
9. Queue storage moves to the heap only by a template flag, for capacities too large for the stack.
10. `SlotRing` is addressed by absolute position, so producers finish out of order and the consumer still reads in order.
11. `ControlChannel` serves a compile-time count of participants. Stop requests are pumped to every participant on a fixed interval.
12. `ViewablePool` holds trivially copyable elements only. Its span is valid until the next reset.
