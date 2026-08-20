# core status

- [x] G1 — zero deps beyond Threads (test-only)
- [x] G2 — SPSC queue proven under TSan + benchmarked
- [x] G3 — event schema stays plain data
- [x] G4 — SpmcRing<T, Capacity, NumConsumers> implemented, gated multi-cursor fan-out

## Last proof

`SpmcRing<T, Capacity, NumConsumers>` (`libs/core/include/spmc_ring.hpp`)
implemented: extends `SpscQueue`'s `construct_at`/`destroy_at` slot
discipline to `NumConsumers` independent cursors, gated so `push()` blocks
rather than overwriting a slot a consumer hasn't read — nothing is ever
silently dropped. First instantiation is `shared_ptr<const MarketEvent>`,
shared via refcount bump rather than each consumer copying the event.
Gained a compile-time-indexed `try_pop<Consumer>()` alongside the runtime
`try_pop(std::size_t)`, for consumers whose cursor is fixed at the wiring
call site rather than assigned at runtime. `qp_core_tests`/`qp_core_bench`
cover it (`test_spmc_ring.cpp`, `bench_spmc_ring.cpp`).

`qp_core_test_support` added (`libs/core/tests/support/`): `ScratchDir`
(RAII temp dir) and `make_book_diff`/`make_trade`/`make_funding`
(`MarketEvent` builders) — both were duplicated near-identically 5x across
sink/source/wire/tests test and bench files; consolidated here since every
lib already depends on `core`, so this avoids the sibling-dependency
problem a lib-level location would've caused. Test-only, unconditional (not
gated by `BUILD_TESTING` — `QP_BUILD_BENCHMARKS`-only consumers need it
too).

`qp_core_tests` passes; TSan clean on the same target (no COMMAND-level
sanitizer-specific test exists — `qp_core_tests` itself is what runs under
the `tsan` preset). `qp_core_bench` measures SPSC push/pop round-trip —
numbers are relative-only per `docs/environment.md`'s WSL caveat, not quoted
here since they drift run to run; re-run `/bench core` for current numbers.
