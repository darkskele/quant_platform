# quant-platform

A modular C++ trading platform where backtest and live run the same code. Venue- and asset-agnostic by design. Binance USD-M perpetuals and medium-frequency holding periods are the current target, not a constraint.

Architecture, design, and milestones: **[docs/README.md](docs/README.md)**.

## Build

Ninja + CMake presets (`CMakePresets.json`), vcpkg manifest mode, C++23.

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

Per-module `qp_<module>_tests` / `_bench` targets exist for narrower runs. Dev in WSL Ubuntu gives relative benchmark numbers only; absolute perf on the VM.

## Data

- Historical data lives in a shared store at `~/quant-data`, symlinked into the repo as `data/` (gitignored). One copy backs every worktree.
- `tools/fetch_backtest_data.sh`: download `data.binance.vision` historical dumps for the fixed symbol universe into the layout the backtest reads.
- `tools/gen_backtest_fixture.py`: regenerate the committed test fixtures under `tests/apps/backtest/fixtures/`.

## Worktrees

Concurrent work runs in git worktrees, one branch per worktree, sharing one `.git` and the one `~/quant-data`.

- `tools/qp-worktree.sh <branch> [base]`: create a sibling worktree at `../quant-platform-<branch>` and link the shared data store into it. Each worktree builds into its own `build/`.

## Backtest

- `tools/build_backtest.sh`: configure and build the backtest for one Source/Sink/Matcher/Risk/Strategy combo and one (symbol, day-range). Data must already be fetched.
- Runs are driven through pybind, not a CLI.

## Benchmark tooling

- `tools/bench/bench_to_md.py`: render a Google Benchmark JSON report as `BENCHMARKS.md`.
- `tools/bench/bench_diff.py`: diff two benchmark reports into a Markdown table of deltas.
