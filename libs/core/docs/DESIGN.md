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
4. ~~**`SpmcRing<T, Capacity, NumConsumers>`: single-producer, multi-consumer, gated.**~~
