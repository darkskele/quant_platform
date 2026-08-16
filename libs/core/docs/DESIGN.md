# core — the lingua franca

Not a city or town — the substrate every city depends on (see root
`docs/DESIGN.md`). Plain data (`MarketEvent`, `EventKind`, `PriceLevel`,
`SymbolId`, `Timestamp`) plus the one piece of real logic every hot path
needs: the SPSC queue connecting an I/O thread to a consumer thread without
locks. Header-only, depends on nothing — everything else depends on this.

## Goals

1. **Zero dependencies.** `qp_core` never links anything but the standard
   library. Anything that would break this (a JSON lib, Boost, a venue type)
   belongs in a lib that depends on `core`, never in `core` itself.
   - **Success metric:** `libs/core/CMakeLists.txt` names no
     `find_package`/`target_link_libraries` dependency beyond `Threads`
     (test-only, for the SPSC queue's concurrent test).
2. **The SPSC queue is correct under real concurrency, not just single-
   threaded logic.** It's the one piece of core with actual state.
   - **Success metric:** `qp_core_tests` passes under TSan; hot-path —
     `qp_core_bench` measures push/pop round-trip (see `docs/environment.md`
     for the WSL-relative/VM-absolute rule).
3. **Event schema stays plain data.** No behavior, no venue-specifics, no
   virtual dispatch — matches `docs/architecture-principles.md`'s "seam 1."
   - **Success metric:** N/A (structural property, not something a test
     proves — checked by review, not a number).
