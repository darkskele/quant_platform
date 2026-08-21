# engine status

- ~~G1 — Compiles against seams only, no concrete adapters~~
- ~~G2 — One full step() proven end-to-end~~

## Last proof

`Engine<Tx,Clk,Exec,Risk,NumWorkers,Strategies...>` (D27) added: `step()`
pulls one event, advances `Clk`, feeds `Exec::on_market_event`, runs every
`Strategy` through `RoundRobinPool` (D33, `libs/core`) — round-robin across
`NumWorkers` compile-time-configured threads — then risk-checks each
resulting `Intent` and submits approved `Order`s **single-threaded, in
strategy-index order** (never arrival order, so decision sequence never
depends on thread scheduling), calls `Risk::on_tick` once, then drains
`Exec::next_outcome()` into `Portfolio` (`Fill` only — `Reject` has no
effect yet). `run()` loops `step()` until the transport is exhausted. No
vtable, no heap allocation on this path (D27) — every constituent type is a
template parameter constrained by its seam's concept, not a virtual base.

`qp_engine_tests`, built entirely from inline test doubles plus the real
`SimClock`/`SimExecution<LastTradeMatcher>`: transport exhaustion (`step()`
→ `false`), full happy path (a `Trade` event sets the matcher's price → a
test strategy's `Intent` → an approved `Order` → a real `Fill` → `Portfolio`
updated), reject path (a non-`Trade` event never sets a price → the same
`Intent`/approval path ends in a `Reject` → `Portfolio` unaffected),
`run()` draining a 3-event transport (a counting strategy sees all 3), and
2 strategies sharing 1 worker both filling and summing correctly in
`Portfolio` — 5/5 passing. Also verified outside the suite: a standalone
compile against the real headers (single/multi-strategy, 1 and 2 workers,
`run()` over 50 events) clean under ThreadSanitizer (ASLR disabled per the
WSL2 TSan workaround, `docs/environment.md`).
