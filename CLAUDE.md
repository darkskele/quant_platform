# CLAUDE.md — project context for Claude Code

## What this is
A modular C++ trading platform where **backtest and live run the same code**. Venue- and asset-agnostic; Binance USD-M perpetuals and medium-frequency are the current target, not a constraint. See `MISSION.md` (why) and `docs/README.md` (how it fits together, with a README + DECISIONS per module below it).

## Non-negotiables
- **One code path.** Backtest and live differ only in compile-time policy types (`Source`, `Transport`, `ExecutionGateway`, `Sink`). Never fork strategy logic on a macro.
- **Never call the system clock in strategy/risk code.** Time arrives on the pull, as `EngineInput::ts`. Determinism landmine.
- **An event is delivered when its information exists.** A bar arrives at its close, never its open. Every mock test passed the open-time stamp. Only an engine-level test through the real source caught it.
- **Strategies emit `Intent`, not venue calls.** They depend only on `MarketEvent` / `Portfolio` / `Intent`, never a concrete adapter.
- **Static dispatch on every seam the software controls** (concepts over CRTP). Virtual dispatch only for a rare, config-selected choice off the hot path; no current seam qualifies. No vtable in the feed loop.
- **Seams first, generality later.** Build concretely; abstract on the second implementation. No plugin framework before a plugin.
- **Performance is a goal.** No heap allocation, needless copy, or indirection on the per-event/per-tick path. Cache-aware layout where it matters. `span`/`string_view`/`constexpr`/move where they apply.
- **Data on disk is binary + zstd + partitioned, never JSON.** Record raw/normalized events, not reconstructed snapshots.
- **Honest costs in `SimExecution`** (fees/funding/slippage/partials). An optimistic fill is how a backtest lies.
- **Risk has autonomous authority** (kill-switch / drawdown flatten).
- **Secrets never committed.** Exchange keys via env or secrets file only.
- **Exchange protocol rules differ by market type (spot vs futures)** even when documented on adjacent pages. Verify against the specific market's own docs. (The resync alignment offset differs between the two; the wrong one passed every mock test and failed 100% against real data.)

## Toolchain
C++23, CMake + Ninja, `CMakePresets.json` (`release`/`debug`/`tsan`; presets are the prod/test switch), dependencies from apt (Boost, OpenSSL, libzip, GoogleTest; `vcpkg.json` exists but is unused), Google Benchmark via FetchContent. Dev in WSL Ubuntu (relative benchmarks only, WSL2 PMU is unreliable); absolute perf on the build box.

## Docs and comments
Follow the `writing` skill for every comment and doc. In short: terse, self-contained, single-source; `docs/` mirrors `include/` with a `README.md` + `DECISIONS.md` per concept directory; module-local decision numbering; no em dashes. Run `clang-format -i` on every `.cpp`/`.hpp` created or edited (never on `CMakeLists.txt`).

## Working style
- Terse. State what changed; don't re-narrate.
- Smallest correct diff. No drive-by cleanup, no speculative abstraction.
- Don't volunteer what was deliberately deferred unless asked.
- Don't caveat with "not built/verified" — the user builds.
- New buildable target? Add its `.vscode/tasks.json` + `launch.json` entries in the same change, unless a wildcard task covers it.

## Git
- **Never `git commit` in this repo**, however the request is phrased. Propose a message, leave the change for the user.
- **Never `git add`, stage, or touch the index** unless staging was explicitly asked. The user works out of staged changes as pre-commit review.
- Branching and pull/rebase only when directly prompted for that action.
- **Cloud sessions are the exception.** Work in a remote container exists nowhere else, so when the user asks, commit to a temporary `claude/` branch and push it for the user to build and run. The final squashed push to a named branch, and deleting the temp branch, also happen on request.

## Build
- **Don't build or run tests/benchmarks unless explicitly asked.** The user runs those. Exception: an iterative "fix X and verify it" step.
- When asked: the `test` and `amend` skills build and run directly via Bash. VS Code's cppdbg/gdb launch is flaky under WSL2; plain execution is what works.

## Build box
The Latitude (`ssh live`, Ubuntu Server 26.04 on real hardware). Setup and tooling live in `~/quant/devenv`, outside this repo.
- **WSL** runs editing, unit tests, sanitizer builds, research. Code lives here only.
- **The box** runs real network and live source tests, and benchmarks whose absolute numbers matter (real PMU, `perf` works).
- **Reach the box only through `boxrun`.** `boxrun [-p preset] [target [args...]]` syncs this worktree, builds there, runs the target. Same build/run permission rule as above.
- **Never edit code on the box.** It holds disposable synced copies under `~/wt/`.
- **Never change the box's system config ad hoc.** Changes go into `~/quant/devenv/box/` scripts, which the user runs (sudo needs their password).
- The box has g++ 15, Boost 1.90, CMake 4.2. WSL has g++ 13, Boost 1.83, CMake 3.28. Code must configure and build on both.
