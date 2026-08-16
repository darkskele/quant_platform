# core status

- [x] G1 — zero deps beyond Threads (test-only)
- [x] G2 — SPSC queue proven under TSan + benchmarked
- [x] G3 — event schema stays plain data

## Last proof

`qp_core_tests` passes; TSan clean on the same target (no COMMAND-level
sanitizer-specific test exists — `qp_core_tests` itself is what runs under
the `tsan` preset). `qp_core_bench` measures SPSC push/pop round-trip —
numbers are relative-only per `docs/environment.md`'s WSL caveat, not quoted
here since they drift run to run; re-run `/bench core` for current numbers.
- As of: working tree, same pass as the `libs/data_source/source`/`libs/venue`
  physical restructure (see root `docs/STATUS.md`).
