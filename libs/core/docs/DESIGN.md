# core — the lingua franca

Not a city or town — the substrate every city depends on (see root
`docs/DESIGN.md`). Plain data (`MarketEvent`, `EventKind`, `PriceLevel`,
`SymbolId`, `Timestamp`) plus the one piece of real logic every hot path
needs: the SPSC queue connecting an I/O thread to a consumer thread without
locks. Header-only, depends on nothing — everything else depends on this.

## Goals

1. ~~**Zero dependencies.**~~ `qp_core` never links anything but the standard
   library. Anything that would break this (a JSON lib, Boost, a venue type)
   belongs in a lib that depends on `core`, never in `core` itself.
2. ~~**The SPSC queue is correct under real concurrency, not just single-
   threaded logic.**~~ It's the one piece of core with actual state.
3. ~~**Event schema stays plain data.**~~ No behavior, no venue-specifics, no
   virtual dispatch — matches `docs/architecture-principles.md`'s "seam 1."
4. **`SpmcRing<T, Capacity>`: single-producer, multi-consumer, gated.**
   Extends the SPSC queue's `construct_at`/`destroy_at` slot discipline
   (goal 2) from one consumer cursor to many independent, per-consumer
   cursors — the producer defers reusing a slot until every registered
   consumer has read past it (backpressures via spin→yield→backoff wait
   instead of ever silently dropping data). First instantiation: `T =
   std::shared_ptr<const MarketEvent>`, letting multiple in-process
   consumers share ownership of one constructed event (a copy-on-read of
   the handle, cheap atomic refcount bump) instead of each copying the
   variable-size, non-trivially-copyable event itself. In-process only —
   cross-process/shared-memory transport is deliberately out of scope until
   there's a concrete operational reason to run consumers in separate
   processes, not before.
