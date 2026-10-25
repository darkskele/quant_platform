# quant-platform

A modular C++ trading platform where backtest and live run the same code. Venue- and asset-agnostic by design. Binance USD-M perpetuals and medium-frequency holding periods are the current target, not a constraint.

Architecture, design, and milestones: **[docs/README.md](docs/README.md)**.

## Build

Ninja + CMake presets (`CMakePresets.json`), C++23. Dependencies come from apt (Boost, OpenSSL, libzip, GoogleTest). Google Benchmark comes through FetchContent.

```bash
cmake --preset release        # or debug, or tsan
cmake --build build/release -j
```

- `release`: optimized, benchmarks on. `build/release`.
- `debug`: ASan/UBSan. `build/debug`.
- `tsan`: ThreadSanitizer, benchmarks off. `build/tsan`.

## Test and benchmark

```bash
ctest --test-dir build/release --output-on-failure     # qp_tests
./build/release/qp_bench --benchmark_min_time=0.1s      # qp_bench
```

Narrow a run with `--gtest_filter` or `--benchmark_filter`. `qp_integration_tests` hits the live archive and is run on purpose, never by ctest. Dev in WSL Ubuntu gives relative benchmark numbers only. Absolute perf comes from the build box.

## Worktrees

Concurrent work runs in git worktrees, one branch per worktree, sharing one `.git`.

- `tools/qp-worktree.sh <branch> [base]`: create a sibling worktree at `../<branch>`, slashes as dashes. Each worktree builds into its own `build/`.

## Hooks and CI

- `git config core.hooksPath .githooks` once per clone. It covers every worktree.
- `pre-commit` regenerates `TODO.md` from the README milestones and code `@todo`s and stages it.
- `.github/workflows/ci.yml` runs on pull requests into main. Every preset builds and runs `qp_tests`, the release build adds the python bindings and `pytest -m "not network"`, and `TODO.md` has to be current. Benchmarks build but do not run.

## Backtest

- Runs are driven through pybind, not a CLI.

## Benchmark tooling

- `tools/bench/bench_to_md.py`: render a Google Benchmark JSON report as `BENCHMARKS.md`.
- `tools/bench/bench_diff.py`: diff two benchmark reports into a Markdown table of deltas.
