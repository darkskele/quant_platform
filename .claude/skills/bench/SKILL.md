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
| `libs/core` | `qp_core_bench` | `libs/core/qp_core_bench` | SPSC queue push/pop round-trip |
| `libs/data_source/source` | `qp_source_bench` | `libs/data_source/source/qp_source_bench` | parse + gap-check + queue push (I/O-thread hot path) |
| `libs/data_source/source` | `qp_file_replay_bench` | `libs/data_source/source/qp_file_replay_bench` | FileReplaySource: single-symbol BookDiff/Trade replay throughput, multi-symbol merge overhead |
| `libs/data_source/source` (venue village) | `qp_venue_bench` | `libs/data_source/source/venue/qp_venue_bench` | Binance message parsing |
| `libs/data_source/wire` | `qp_wire_bench` | `libs/data_source/wire/qp_wire_bench` | wire format write/read/round-trip |
| `libs/data_source/transport` | `qp_transport_bench` | `libs/data_source/transport/qp_transport_bench` | InProcessTransport::next() vs bare ring; CombinedTransport dispatch overhead, single and 2-source round-robin |
| `libs/strategy` (carry village) | `qp_carry_bench` | `libs/strategy/carry/qp_carry_bench` | FundingCarryStrategy::on_event: entry/hold/reject paths |
| `libs/risk` | `qp_risk_bench` | `libs/risk/qp_risk_bench` | BasicRiskGate::check (approve/resize), on_tick (no-drawdown fast path) |

Binary output paths mirror the source tree, not always `libs/<lib>/<target>`
— use the Path column, not a guessed pattern.

## Selecting what to run

Same diff-based targeting as `test` — union of `git status --short` and
`git diff --staged --name-only`, map changed paths to the table by
`libs/<name>/` (nested villages map to their town's row), pull in anything
transitively depending on a changed lib (`libs/core` changed → rerun every
bench). Doesn't map cleanly (top-level files, several unrelated libs,
nothing changed) — fall back to **all**. `cmake --build build/release
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
3. Run the binary directly — not through ctest, these aren't pass/fail:
   `./build/release/qp_bench --benchmark_min_time=0.1s` for `all`, or
   `./build/release/<Path column> --benchmark_min_time=0.1s` per target
   (matches `launch.json`'s bench configs).
4. Report the actual per-case numbers (ns/op, items/sec), not just "ran
   successfully." Note a delta against an earlier number from *this*
   session if one exists; otherwise just report current numbers — don't
   invent a baseline.
