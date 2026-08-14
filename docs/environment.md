# Environment & tooling

## Path: WSL Ubuntu now → VM later

Develop initially on the laptop in **WSL Ubuntu**; move to a **VM** once there's
something worth deploying (the collector goes to a VM early — it needs 24/7
uptime). WSL Ubuntu and VM Ubuntu share a toolchain, so migration is nearly
free.

## Build

- **CMake + Ninja**, driven by **`CMakePresets.json`** — this is where the
  prod/test/backtest **compile-time switch lives**. Each preset selects a config
  header + policy set, so `cmake --preset backtest` and `--preset live` build the
  *same* engine with different instantiations.
- **vcpkg (manifest mode)** — `vcpkg.json` pins exact dependency versions so
  laptop and VM resolve identically. Needs: WebSocket client, fast JSON parser
  (ingest only), zstd, GoogleTest, Google Benchmark.
- **Compiler:** recent clang/gcc, C++20/23. Prefer **concepts** over raw CRTP
  for policy seams (same zero-cost dispatch, better errors).
- **Sanitizers** (ASan/UBSan, TSan for concurrency) wired into a debug preset —
  one flag away, not an afterthought.
- **Dockerfile pinning the toolchain** kept from early on — not necessarily to
  develop inside, but so "builds on laptop" provably == "builds on VM."

## Performance engineering — the WSL caveat

**WSL2 has limited/unreliable hardware perf-counter (PMU) access.** Cache-miss /
branch-mispredict / cycle counters often aren't exposed to `perf`, so absolute
latency numbers measured under WSL2 are suspect.

- Use **WSL for correctness + relative microbenchmarks** (Google Benchmark works
  fine — trust *deltas*).
- Use the **Linux VM / bare metal as the reference for absolute perf numbers.**
- Tools: **Google Benchmark** (anywhere), **perf + flamegraphs** (VM — PMU
  limited under WSL2), **Valgrind/callgrind** (anywhere, slow), **HdrHistogram**
  (latency tails, not means), **Compiler Explorer / `-S`** (check the compiler
  did what you think).

Rule: trust deltas in WSL, take absolutes on the VM.

## Agentic assist (Claude Code)

- Installs/runs natively in WSL Ubuntu (npm). **Included in Claude Pro** — no
  extra charge; usage counts against shared Pro limits (heavy sessions can hit
  the cap → wait for reset, or Max, or opt-in API credits).
- Reads **`CLAUDE.md` at repo root** for standing context (this repo's
  conventions + `docs/`). Project skills / slash commands live under `.claude/`.
- All local; no cloud dependency to work against the repo.

## Version control

- **Git, fully local, from commit #1** — no remote required (`git init`, commit
  freely).
- **But local-only = no backup.** Push to a **private** remote (GitHub/GitLab
  private, or a bare repo on the VM) periodically — a working edge isn't
  open-sourced, but a laptop failure must not erase months of work *and*
  irreplaceable collected data.
- **Secrets discipline from commit #1.** This repo will hold exchange API keys.
  `.gitignore` + env-var/secrets-file loading set up *before* any key exists, so
  committing a credential is never possible. A leaked trading key = drained
  account.
