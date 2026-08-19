# Root design — engineering process & infrastructure

This is the root level of the city/town/village doc hierarchy (see
`docs/repo-layout.md` for the convention itself). It doesn't restate the
product mission — that's [`MISSION.md`](MISSION.md) — or the high-level
seams — that's [`docs/architecture-principles.md`](docs/architecture-principles.md).
This file states the root's *other* standing concern: that the engineering
process itself — docs, build, tests, benchmarks, comment standard — is
provably in good order at every level, not just the product code.

Trigger: repeated regressions this session (two `PUBLIC`/`PRIVATE` CMake
bugs, D13's resync protocol bug) that better structure and tighter proof
loops would have caught earlier or prevented outright.

## Cities today

Two, coequal — independent seams (`Source` vs `Sink`, separate
consumers), physically grouped as sibling directories under `libs/data_source/`
for filesystem convenience only, not merged into one concern:

- **source** — everything behind `Source`. Today: the **prod
  streamer** town (`libs/data_source/source/`) — generic resync/gap-detection
  machinery plus the live Boost.Beast transport (`GenericLiveWebSocketSource`)
  and the backtest replay `Source` (`FileReplaySource`), both town-level (D18)
  — with one **venue** village (`venue/`, Binance glue) nested below.
- **sinks** (`libs/data_source/sink/`) — everything behind `Sink`/`Recorder`. No
  further nesting — one `Sink` implementation (`FileRecorder`) today.

`libs/core/` is the lingua franca substrate both cities depend on — not
itself a city. `libs/data_source/wire/` is a second substrate, scoped to
these two cities rather than the whole repo: the on-disk `MarketEvent` codec
(`wire.hpp`/`zstd_stream.hpp`/`partition.hpp`) `source` and `sink` both
depend on instead of on each other (D19). `apps/collector/` composes prod
streamer + `FileRecorder` ("what it's made of") — described at root, not a
city/town itself.
Execution/risk/strategy/engine (per `docs/repo-layout.md`'s planned tree)
arrive with the trader milestone — not built yet, not goals here.

## Goals (this initiative)

1. **Doc hierarchy matches code hierarchy.** City → town → village → leaf.
   Non-leaf directories get `DESIGN.md` (mission/goals/success-metrics for
   that level) + `STATUS.md` (milestones, last proof, tagged to commit).
   Leaves get code comments only, no doc file. Decisions are logged nearest
   the code they concern; a superseded decision is struck through
   (Markdown `~~...~~`) with a pointer to where it moved, not deleted.
   - **Success metric:** every non-leaf directory has both files.
     `docs/decisions.md`'s marketdata/record-specific entries (D10, D11,
     D12, D13, D14) relocated to their nearest new home, struck through at
     the old location.

2. **`libs/data_source/source` becomes venue-agnostic.** `LiveWebSocketSource`
   templated on a minimal `Parser` concept (C++20, structural) capturing
   exactly what it calls on `venue::binance::*` today — nothing speculative
   added for a venue that doesn't exist. `ResyncCoordinator::on_snapshot`
   narrowed from the full `venue::binance::DepthSnapshot` to just
   `last_update_id`, the only field it uses.
   - **Success metric:** `qp_source_tests`, `qp_source_integration_tests`,
     `qp_collector_integration_tests` all pass; TSan clean on the
     multi-threaded ones; no `venue::binance::` name appears inside the
     generic transport/resync code paths — only inside the concept-satisfying
     implementation and `apps/collector`'s own composition.

3. **Build granularity matches the hierarchy.** Every town/village/leaf
   buildable and testable individually, and testing a town includes its
   children's tests.
   - **Success metric:** one command per level runs that level's full test
     set, children included (e.g. testing the `source` town runs
     `qp_source_tests` + `qp_source_integration_tests` +
     `qp_venue_tests`). Same for benchmarks.

4. **Benchmarks are required for hot-path code, not optional.** Cold-path
   components may state N/A in their `DESIGN.md`'s success metrics — but
   that's a stated, deliberate exemption, never silence.
   - **Success metric:** every current hot-path component already has one
     (parser, gap detector, SPSC queue, wire format) — stays true as new
     hot-path code is added, checked by `/audit`.

5. **Agentic support stays token-conscious.** Decisions terse for agent
   consumption; status highly formatted but bulleted/terse; a `/audit`
   skill reads only what's needed (a level's `DESIGN.md` goals + `STATUS.md`
   last proof + targeted test/bench output) rather than the whole tree.
   - **Success metric:** `/audit` exists, deriving scope from staged
     changes (no path arg), explicit-invocation-only (same standing rule as
     `/test`/`/bench` — never self-triggered), produces a terse per-goal
     pass/fail report.

6. **Baseline stays green.** All tests and all benchmarks pass — not a
   one-time check, the standing bar `STATUS.md` reports against.

## Also in scope (smaller, same pass)

- **CMake audit**: re-verify the refactor in (2) doesn't reintroduce a
  `PUBLIC`/`PRIVATE` or `find_package`-scoping bug (both now documented in
  `docs/environment.md`); consider a "public header self-containment" build
  check; look at silencing the recurring cosmetic RPATH warning properly.
- **Comment standard**: Doxygen-compatible tag syntax (`///`, `@param`,
  `@return`, `@pre`, `@warning` — clangd renders these as hover tooltips),
  applied per the existing terse/why-only philosophy (`CLAUDE.md`), not
  Doxygen's typical exhaustive default. New code going forward, not a
  retrofit.
- **Computational-latency door stays open.** A line in
  `docs/architecture-principles.md` distinguishing "not competing on
  network/hardware latency" from "computational efficiency doesn't
  matter" — SIMD/cache-conscious/lock-free work is welcome (Phase 4's
  feature engine is the natural home), just not infrastructure racing.
- **README.md**: stale ("design locked, Phase 0 not started") — needs
  real build/run instructions for what exists now, written extensibly for
  future apps.
