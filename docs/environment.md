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
  (ingest only), zstd, GoogleTest, Google Benchmark. Wired into root
  `CMakeLists.txt`: exporting `VCPKG_ROOT` before configuring switches every
  `find_package()` call from local apt/conda packages to vcpkg, unchanged —
  unset, it's a no-op (today's state on this WSL box: vcpkg itself isn't
  bootstrapped yet, blocked on `zip`/`unzip` not being installed and no
  passwordless `sudo` to install them — `sudo apt-get install zip unzip`,
  then `./bootstrap-vcpkg.sh` in a cloned `microsoft/vcpkg`, then export
  `VCPKG_ROOT` to that clone).
- **Compiler:** recent clang/gcc, C++20/23. Prefer **concepts** over raw CRTP
  for policy seams (same zero-cost dispatch, better errors).
- **Sanitizers** (ASan/UBSan, TSan for concurrency) wired into a debug preset —
  one flag away, not an afterthought.
- **TSan under WSL2** can fail with `FATAL: ThreadSanitizer: unexpected
  memory mapping` — an ASLR incompatibility, not a real race. Workaround:
  disable ASLR for the run, `setarch $(uname -m) -R ./build/tsan/<binary>`.
- **`target_link_libraries` PUBLIC vs PRIVATE tracks the header, not the
  `.cpp`.** A dependency is PUBLIC whenever any of the target's own public
  headers name that dependency's types in their interface — PRIVATE only
  when it's confined entirely to the implementation. Getting this backwards
  compiles fine until a new downstream consumer includes the public header
  and can't find the transitive include path — bit twice in one session
  (`qp_venue` on `qp_source`, `zstd::libzstd` on `qp_sink`), both only
  surfacing once `apps/collector` became a second consumer. A lib with no
  downstream consumer yet ships this silently.
- **CMake audit, post-restructure** (`libs/venue` → `libs/data_source/source/venue/
  binance`, websocket transport → `libs/data_source/source/protocol`): the
  PUBLIC/PRIVATE rule above re-verified across every lib, no regression.
  Header self-containment checked manually — all 14 public headers compile
  standalone (`-fsyntax-only` against the real include flags from
  `compile_commands.json`); not wired into CMake as a standing target, rerun
  manually if a regression is ever suspected. The recurring RPATH warning
  (`libssl.so.3` "may be hidden") is dev-box-specific — this box has both
  `/usr/lib/x86_64-linux-gnu/libssl.so.3` and a conda one; `ldd` on the built
  binary confirms the conda one resolves at runtime, the same one CMake
  linked against — benign, not chased with a box-specific RPATH override
  that could behave differently on the eventual VM/Docker target.
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

**Variance**: `--benchmark_repetitions=N` (Google Benchmark's own mean/
median/stddev/cv aggregates), not a hand-rolled per-iteration sampler — the
latter's clock-call overhead and WSL2/Hyper-V scheduling noise swamp the
signal (D51). `tools/bench/bench_to_md.py` renders a
`--benchmark_out_format=json` report to Markdown for a root `BENCHMARKS.md`.

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
