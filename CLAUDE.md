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
  (`MarketDataSource`, `Clock`, `ExecutionGateway`, `Sink`). Never fork strategy
  logic on a macro.
- **Never call the system clock in strategy/risk code** — always the injected
  `Clock` seam. This is a determinism landmine.
- **Strategies emit `Intent`, not venue calls.** They depend only on
  `MarketEvent` / `StateView` / `Intent` — never on concrete adapters.
- **Static dispatch on hot/fixed seams (concepts preferred over CRTP); virtual
  on cold/runtime seams (strategy, risk).** Don't templatize the rebalance path;
  don't put a vtable in the feed loop.
- **Seams first, generality later.** Build concretely; abstract on the 3rd
  implementation. No plugin framework before a plugin.
- **Data on disk is binary + zstd + partitioned. Never JSON.** Record
  raw/normalized events, not reconstructed snapshots.
- **Honest costs in `SimExecution`** (fees/funding/slippage/partials). An
  optimistic fill sim is how backtests lie.
- **Risk has autonomous authority** (kill-switch / drawdown flatten).
- **Secrets never committed.** Exchange keys via env/secrets file only.

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
