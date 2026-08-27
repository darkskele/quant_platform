# transport decisions

## D39 — New `transport` city; completes D22's `Source`→`Transport` rename for the seam `Engine` consumes
`docs/decisions.md` D22 renamed the *concept* `Engine` depends on from
`Source` to `Transport`, but left `source.hpp`'s `Source` concept itself
unrenamed and deferred building the actual `Transport` type — no in-process
adapter existed yet to motivate it (see `libs/engine/docs/DECISIONS.md`
D30). It's motivated now: `funding-carry`'s planned spot+perp trade needs
`Engine` to consume two live venue connections through one `Tx`, and the
old `InProcessTransport` (`source/include/queue_source.hpp`) didn't even
structurally satisfy `Source` as written (`optional<shared_ptr<const
MarketEvent>>` vs the required `optional<MarketEvent>`) — a real,
previously-flagged gap, not a rename exercise.

Resolved with a new top-level city, `libs/data_source/transport`, sibling
to `source`/`sink`/`wire`: `transport.hpp` (the `Transport` concept, its
own file per D27-style seam hygiene), `in_process_transport.hpp`
(`InProcessTransport`, moved and fixed to actually satisfy the concept —
dereferences/copies out of the ring's `shared_ptr` slot), and
`combined_transport.hpp` (`CombinedTransport<Sources...>`, new — a
round-robin merge over a closed, compile-time set of `Transport`s).

For the multi-venue live case specifically, `CombinedTransport` often isn't
even needed: if two venues' fan-out rings share the exact same `SpmcRing<T,
Capacity, NumConsumers>` instantiation, `InProcessTransport<Ring, 0>` for
each venue is the same C++ type, storable directly — no tuple, no
type-erasure. `CombinedTransport` covers the general case (heterogeneous
`Transport` types, or backtest wiring a spot+perp replay pair into one
`Tx`) via a compile-time function-pointer dispatch table
(`std::array<NextFn, N>` built from a fold over `std::index_sequence`,
matching the fold-dispatch idiom `RoundRobinPool`/`Engine::make_pool`
already use) — no vtable, per D27.

`Engine`'s `Tx` is now constrained on `transport::Transport`, not
`source::Source`; `qp_engine` links `qp_transport` instead of
`qp_source_pure`. `source::Source` itself stays unrenamed — still no
reason to touch it, same as D22/D30 already decided; the two concepts stay
structurally identical, deliberately named for their own city.

## ~~D42 — `OffsetTransport<Tx, Offset>`: two legs share one `SymbolId` space via a compile-time offset, not a shared `SymbolTable`~~
**Superseded by D43** (`libs/data_source/sink/docs/DECISIONS.md`) —
`MarketEvent` gained a `venue` field, stamped by `FanoutSink::record<I>()`
at the point events are pushed into their leg's ring. `(symbol, venue)`
identifies an instrument directly; no `SymbolId` numbering scheme (offset
ranges or otherwise) is needed at all. `offset_transport.hpp` removed.

`FundingCarryStrategy` needs to hold spot and perp positions independently
in one `Portfolio`, but each leg's `GenericLiveWebSocketSource`/
`FileReplaySource` owns its own private `SymbolTable` — both would
independently intern "BTCUSDT" to `SymbolId 0`, colliding once merged
(flagged as a deferred problem in `apps/collector/docs/DECISIONS.md` D41,
resolved by D43).

Considered and rejected: a single `SymbolTable` shared across both legs,
qualifying each leg's raw exchange string before interning (e.g.
`"PERP:BTCUSDT"`/`"SPOT:BTCUSDT"`). Rejected because it needs a per-message
allocation to build the qualified string for the lookup — `SymbolTable::
intern`'s steady-state cost is deliberately allocation-free today
(`venue_types.hpp`), and this would regress that on the I/O hot path. It
would also require `GenericLiveWebSocketSource`/`FileReplaySource` to take
an externally-owned `SymbolTable&` instead of owning one privately — a
real, unnecessary structural change to already-working code.

Resolved instead, briefly, with `offset_transport.hpp`: `OffsetTransport<Tx,
Offset>` wrapped any `Transport`, adding a compile-time-constant `SymbolId`
offset to every event before returning it — no allocation, no string
handling, no change to any `Source`'s ownership model, but still required
picking non-overlapping offset ranges by hand (a caller invariant the type
system couldn't check). D43's `venue` field is strictly simpler: no
numbering scheme to get right at all.
