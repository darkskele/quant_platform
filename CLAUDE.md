# CLAUDE.md — project context for Claude Code

## What this is
A modular C++ platform for **medium-frequency crypto trading** (Binance USD-M
perpetuals) where **backtest and live run the same code**. Backtest research
suite + live trading bot. Goal: genuine risk-adjusted profitability from
structural/statistical edges — **not HFT**. See `MISSION.md`.

## Read these before working
- `MISSION.md` — north star, success tiers, non-goals.
- `docs/architecture-principles.md` — seams, the Engine composition, the rules.
- `docs/strategy.md` — strategy families and sequencing.
- `docs/data.md` — data sources, collector, storage.
- `docs/environment.md` — build, perf, tooling, VCS, secrets.
- `docs/roadmap.md` — phases and current focus.
- `docs/decisions.md` — decision log (append new decisions here).

## Non-negotiable conventions
- **One code path.** Backtest/live differ only in compile-time policy types
  (`Source`, `Clock`, `ExecutionGateway`, `Sink`). Never fork strategy
  logic on a macro.
- **Never call the system clock in strategy/risk code** — always the injected
  `Clock` seam. This is a determinism landmine.
- **Strategies emit `Intent`, not venue calls.** They depend only on
  `MarketEvent` / `StateView` / `Intent` — never on concrete adapters.
- **Static dispatch everywhere the software controls the cost** (concepts
  preferred over CRTP) — including strategy and risk. Virtual dispatch is
  reserved for a genuinely rare, config-selected choice with no hot-path
  exposure; none of the current seams qualify (D27). Don't put a vtable in
  the feed loop.
- **Seams first, generality later.** Build concretely; abstract on the 3rd
  implementation. No plugin framework before a plugin.
- **Data on disk is binary + zstd + partitioned. Never JSON.** Record
  raw/normalized events, not reconstructed snapshots.
- **Honest costs in `SimExecution`** (fees/funding/slippage/partials). An
  optimistic fill sim is how backtests lie.
- **Risk has autonomous authority** (kill-switch / drawdown flatten).
- **Secrets never committed.** Exchange keys via env/secrets file only.
- **Exchange protocol rules differ by market type (spot vs. futures) even
  when documented on adjacent pages.** Never assume one documents the
  other — verify against the specific market's own docs. (D13: the resync
  alignment offset differs between spot and futures; implementing the
  wrong one passed every mock/synthetic test and failed 100% of the time
  against real data.)

## Toolchain
- C++20/23, CMake + Ninja, `CMakePresets.json` (presets ARE the prod/test
  switch), vcpkg manifest mode, GoogleTest + Google Benchmark, ASan/UBSan/TSan
  debug preset.
- Dev in WSL Ubuntu (relative benchmarks only — WSL2 PMU is unreliable);
  absolute perf numbers on the VM.

## Current focus
See the top uncompleted phase in `docs/roadmap.md` (starts at Phase 0).

## Working style
- Terse answers. State what changed; don't re-narrate docs already read.
- Decisions/architecture/roadmap changes go in the owning doc, not just chat —
  use the `amend-doc` skill. `docs/decisions.md` is append-only; other docs
  get minimal in-place edits, not rewrites.
- Smallest correct diff. No drive-by cleanup, no speculative abstraction.
- Don't volunteer what was deliberately left out/deferred unless asked.
- Don't caveat with "this hasn't been built/verified" — the user builds
  (see Build workflow); just say what changed.
- Run `clang-format -i` on every C++ file created or edited, every time —
  don't rely on an editor's format-on-save.
- **Comment standard: Doxygen-compatible tags** (`///`, `@param`, `@return`,
  `@pre`, `@warning`) on public function/method declarations where there's
  something non-obvious to say — clangd renders these as hover tooltips, so
  they pay for themselves in the editor, not just in generated docs. This
  does NOT mean Doxygen's typical exhaustive default (a tag block on every
  function regardless of whether it says anything) — default to no comment;
  add one only when the WHY is non-obvious, same bar as any inline comment.
  New code going forward; not a retrofit of existing comments.
- Adding a new buildable target (test/bench/app executable)? Add its
  `.vscode/tasks.json` build+test entries and `launch.json` debug config in
  the same change, matching the existing per-target pattern — unless a
  generic/wildcard task already covers it.

## Git workflow
- **Never `git commit`, in this repo, regardless of mode or how the request
  is phrased.** At most, propose a commit message and leave the change
  staged/unstaged for the user to commit themselves.
- Branching and pull/rebase are fine, but only when directly prompted for
  that specific action — not as a side effect of finishing other work.

## Build workflow
- **Don't build or run tests/benchmarks unless explicitly asked** — the user
  runs those themselves. Exception: when told to as part of an iterative
  change (e.g. "fix X and verify it"), building/running for that specific
  step is fine.
- When asked to build/run tests, use the `test` skill (`/test`); when asked
  to run benchmarks, use the `bench` skill (`/bench`) — both build and run
  directly via Bash (ctest / Google Benchmark), targeted to what changed by
  default. Not a standing exception to the rule above — still only on
  explicit ask, this is just the mechanism. (VS Code's cppdbg/gdb
  debug-launch is currently broken/flaky under WSL2 in this window — plain
  execution is what's confirmed working.)
