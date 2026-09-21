---
name: test
description: Wipe the build folder, then run every launch in .vscode/launch.json through its build task via Bash, sanitizers and benches included. Fixes each failure with the smallest possible change, summarizes the benches, and reports regressions that survive drift correction, the runs' spread, and a rerun. Explicit invocation only (/test), never proactively.
---

Explicit invocation only (`/test`). Never run on your own judgment because code changed.

The launches in `.vscode/launch.json` are the contract. Every one of them has to build and pass from nothing. Run them via Bash, never through VS Code or gdb; plain execution is what works here.

## Steps

1. `rm -rf build`. The whole folder, every preset. A clean build is what surfaces every warning.
2. Save the committed bench baseline to the scratchpad: `git show HEAD:bench_results.json > <scratchpad>/bench_head.json`.
3. For each configuration in `.vscode/launch.json`, in file order:
   - Run its `preLaunchTask`'s `command` from `.vscode/tasks.json`, exactly as written. Capture every compiler warning and error. A warning is a failure.
   - Then run what the launch runs, from the repo root:
     - `cppdbg`: `program`, with each `environment` entry set as an env var. A program under `build/tsan/` runs under `setarch $(uname -m) -R`, since TSan cannot map its shadow with ASLR on under WSL2. gdb turns ASLR off itself, which is why the launch does not say so.
     - `debugpy`: the launch's `python` interpreter on `program`.
     - `node-terminal`: `command`.
   - Integration tests, sanitizer runs and benches take minutes. Run those in the background and log to the scratchpad.

## On any failure, warning, sanitizer report, or error

1. Fix it with the smallest possible change.
2. Rebuild and rerun that launch to confirm the fix holds.
3. Report what broke and what changed, per fix, with the real failing output, not just counts.

## Benches

The bench launch runs in release; under a sanitizer a bench measures the sanitizer. It bakes in five repetitions and rewrites `BENCHMARKS.md` and `bench_results.json`. This is WSL2, so trust deltas between runs, never a number as absolute truth.

- `python3 tools/bench/bench_diff.py <scratchpad>/bench_head.json bench_results.json`. It divides out the suite-wide drift, then keeps only deltas past the noise floors and outside 2 sigma of the two runs' spread.
- Rerun each flagged regression twice on its own: `./build/release/qp_bench --benchmark_filter='^<name>$' --benchmark_out=<scratchpad>/rerun_<n>.json` (the later `--benchmark_out` wins, so the tracked files are untouched). A regression counts only if both reruns stay past the baseline by the same test.
- For a confirmed one, check whether the code on its path changed since `HEAD`, and tie it to that change or call it unexplained.

## Report

- One line per launch, pass or fail with its counts.
- The fixes.
- The benches: an overview grouped by component with the headline numbers, not a raw dump, then the confirmed regressions with their rerun numbers.

Terse.
