---
name: bench
description: Build and run this repo's Google Benchmark suites directly via Bash, release preset. Same reasoning as the `test` skill (VS Code's cppdbg/gdb debug-launch is currently broken/flaky under WSL2 in this window). Targeted-by-default (only what current changes affect), or "all", or a specific lib/target. ONLY invoked when the user explicitly asks (/bench or "run the benchmarks") — never proactively, per CLAUDE.md's Build workflow rule.
---

Explicit invocation only (`/bench`, `/bench all`, `/bench <lib>`) — never on
own judgment just because code changed. Always **release**
(`build/release`), never debug — Google Benchmark under ASan/UBSan measures
sanitizer overhead, not the code (`docs/environment.md`). Even in release
this is WSL2: trust deltas between runs/versions, never report a number as
absolute truth.

`qp_bench` links every lib's benchmark sources into one binary, one results
table — the `all` mechanism. Per-lib targets below still exist for targeted
runs.

## Targets (update this table when a bench target is added or renamed)

| Lib | Bench target | Path | Measures |
|---|---|---|---|
| `core` | `qp_core_bench` | `cmake/core/qp_core_bench` | SpscQueue/SpmcRing/MpscQueue/RoundRobinPool (isolation/tandem/contention), ControlChannel (isolation/tandem, no contention — see `benchmarks/README.md`), Portfolio (isolation only) |
| `clock` | `qp_clock_bench` | `cmake/clock/qp_clock_bench` | SimClock now()/advance() — deliberate suite-wide-consistency exception, see `benchmarks/README.md` |
| `execution` | `qp_execution_bench` | `cmake/execution/qp_execution_bench` | LastTradeMatcher (on_market_event, fill/reject paths), SimExecution (submit+next_outcome tandem) |
| `data_source/source` | `qp_source_bench` | `cmake/data_source/source/qp_source_bench` | GapDetector, SymbolTable (venue_types.hpp), ExponentialBackoff, FuturesAlignment/SpotAlignment, ResyncCoordinator/find_resync_point |
| `data_source/source` | `qp_file_replay_bench` | `cmake/data_source/source/qp_file_replay_bench` | FileReplaySource: single-symbol BookDiff/Trade replay throughput, multi-symbol merge overhead |
| `data_source/source` (venue village) | `qp_venue_bench` | `cmake/data_source/source/venue/qp_venue_bench` | Binance message parsing |
| `data_source/wire` | `qp_wire_bench` | `cmake/data_source/wire/qp_wire_bench` | wire format write/read/round-trip |
| `data_source/sink` | `qp_sink_bench` | `cmake/data_source/sink/qp_sink_bench` | FanoutSink (isolation/tandem/contention), FileRecorder (isolation — real writer thread is inherent to the class) |
| `data_source` | `qp_data_source_bench` | `cmake/data_source/qp_data_source_bench` | run_data_source() poll-round throughput, 1 and 4 source/sink pairs |
| `engine/transport` | `qp_transport_bench` | `cmake/engine/transport/qp_transport_bench` | BacktestInProcessTransport two-ring timestamp merge (isolation+tandem only — no contention variant yet, see `benchmarks/README.md`) |
| `strategy` (carry village) | `qp_carry_bench` | `cmake/strategy/carry/qp_carry_bench` | FundingCarryStrategy::on_event: entry/hold/reject paths |
| `risk` | `qp_risk_bench` | `cmake/risk/qp_risk_bench` | BasicRiskGate::check (approve/resize), on_tick (no-drawdown fast path) |

Binary output paths mirror each target's own `cmake/<lib>/CMakeLists.txt`
location (D50: CMakeLists.txt files live under `cmake/`, mirroring the
content-type roots) — use the Path column, not a guessed pattern.

No `apps/backtest` or `apps/collector` rows, on purpose: `apps/` is thin
composition wiring, not its own thing to benchmark — every component
either one wires together already has its own row above (D53,
`docs/decisions.md`). `data_source/transport` (a separate, older
`Transport`/`InProcessTransport` scaffold) is also **not** in this table on
purpose — its `cmake/data_source/CMakeLists.txt` never `add_subdirectory`s
it, so it's dead/orphaned, not part of the actual build (its own bench
source even `#include`s a header that no longer exists). Flagged to the
user; not cleaned up as part of benchmarking work.

`GenericLiveWebSocketSource::on_message()` also has no bench row: it's
private, only reachable by standing up a real mock WS+HTTP server per
iteration, which times server/socket/handshake setup, not the parse/
gap-check/queue-push logic actually worth measuring. A first attempt did
this and was deleted (D53) — it measured harness overhead, not the class.
Needs a throughput-style harness (wall-clock over N messages, not GBench's
per-iteration model), not a patch on the same shape.

## Selecting what to run

Same diff-based targeting as `test` — union of `git status --short` and
`git diff --staged --name-only`, map changed paths to the table by
`<name>/` at the repo root (nested villages map to their town's row), pull
in anything transitively depending on a changed lib (`core` changed →
rerun every bench). Doesn't map cleanly (top-level files, several unrelated
libs, nothing changed) — fall back to **all**. `cmake --build build/release
--target qp_prod_streamer_bench -j` builds `qp_source_bench` +
`qp_file_replay_bench` + `qp_venue_bench` together when all are in scope.

- **No argument (default)**: targeted, by current changes.
- **`all`**: build+run `qp_bench` directly, ignore git state and the table.
- **A lib or target name** (`/bench wire`, `/bench qp_wire_bench`): just
  that one, from the table.

## Steps

1. `cmake --preset release` — configure; cheap no-op if unchanged.
2. `all`: `cmake --build build/release --target qp_bench -j`. Targeted:
   `cmake --build build/release --target <selected targets> -j`.
3. Run the binary directly — not through ctest, these aren't pass/fail.
   For `all`: `./build/release/qp_bench` — no flags needed.
   `benchmarks/qp_bench_main.cpp` bakes in `--benchmark_repetitions=5
   --benchmark_report_aggregates_only=true` as defaults (still overridable)
   and regenerates the root `BENCHMARKS.md` on every run
   (`tools/bench/bench_to_md.py`, no separate step) — this is the one
   binary meant to be run standalone. For a targeted subset through the
   same binary, `./build/release/qp_bench --benchmark_filter=<pattern>`
   still regenerates the MD, just for what ran. Per-lib targets
   (`./build/release/<Path column>`) use stock `benchmark_main` — no
   defaults, no MD regeneration — pass `--benchmark_min_time=0.1s
   --benchmark_repetitions=5 --benchmark_report_aggregates_only=true`
   by hand (matches `launch.json`'s bench configs) for a fast targeted
   look without touching the checked-in MD file.
4. Report the actual per-case numbers (ns/op, items/sec), not just "ran
   successfully." Note a delta against an earlier number from *this*
   session if one exists; otherwise just report current numbers — don't
   invent a baseline.
