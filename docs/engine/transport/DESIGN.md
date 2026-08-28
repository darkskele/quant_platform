# transport — the "transport" city

Sibling to **source**/**sink**/**wire** (root `docs/DESIGN.md`), living
under `engine` rather than `data_source`. `Transport` (`transport.hpp`) is
the seam `Engine` actually consumes — one `MarketEvent` at a time, or
`nullopt` — structurally identical to `source`'s `Source` concept (D22: the
two cities keep their own names; anything satisfying one satisfies the
other, no declared relationship). One implementation today:
`BacktestInProcessTransport<Ring, N, NumConsumers>` (merges a fixed set of
fan-out rings in `MarketEvent::ts` order, for multi-leg cases like a carry
strategy's spot+perp). `CombinedTransport` (round-robin merge) removed, no
real caller ever used it (D52). No further nesting.

Depends on `core` only (`MarketEvent`, `SpmcRing`) — never on `source` or
`sink`, matching `data_source`'s coequal-cities rule.

## Goals

1. ~~**`Transport` concept + `InProcessTransport`**~~ — moved out of
   `source`'s old `queue_source.hpp` (D38-adjacent, see this lib's
   `DECISIONS.md`), fixed to structurally satisfy `Transport`
   (`optional<MarketEvent>`, not `optional<shared_ptr<const MarketEvent>>`).
2. ~~**`CombinedTransport`**~~ — generic round-robin merge over a closed,
   compile-time set of `Transport`s, so `Engine` only ever holds one `Tx`
   regardless of how many underlying sources feed it.
