---
name: bench
description: Build and run every Google Benchmark suite in release via Bash, then summarize. Explains latency fluctuations against the committed baseline when there is a git diff. Explicit invocation only (/bench), never proactively.
---

Explicit invocation only (`/bench`). Never run on your own judgment.

Always release (`build/release`); benches under ASan/UBSan measure the sanitizer, not the code. This is WSL2: trust deltas between runs and versions, never a number as absolute truth.

## Steps

1. `cmake --preset release`.
2. `cmake --build build/release --target qp_bench -j`.
3. `./build/release/qp_bench`. One binary, every case. It bakes in `--benchmark_repetitions=5 --benchmark_report_aggregates_only=true` and regenerates the root `BENCHMARKS.md` (`tools/bench/bench_to_md.py`).

## Report

- An overview grouped by component: the headline numbers, not a raw dump.
- If there is a git diff, explain the latency picture against the committed baseline. `tools/bench/bench_diff.py` gives the deltas. Tie a real move to the code that changed, and separate it from WSL2 noise: a delta clearing neither the relative nor the absolute floor is noise, not a regression.
