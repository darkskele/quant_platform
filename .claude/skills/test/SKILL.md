---
name: test
description: Build and run every GoogleTest suite from a clean build in both debug and release, via Bash (cmake + ctest). Reports failures, warnings, and errors, fixes them, and reports each fix. Explicit invocation only (/test), never proactively.
---

Explicit invocation only (`/test`). Never run on your own judgment because code changed.

Run every suite, both presets, from a clean build. Build via Bash (`cmake` + `ctest`), never a VS Code task or gdb; plain execution is what works here. A clean build is what surfaces every warning (`-Wall -Wextra` are on; an incremental build skips already-compiled units).

## Steps

1. `rm -rf build/debug build/release`.
2. For each preset in `debug`, then `release`:
   - `cmake --preset <preset> -DQP_BUILD_BENCHMARKS=OFF` (skips the benchmark fetch; tests don't need it).
   - `cmake --build build/<preset> --target qp_all_tests -j`. Capture every compiler warning and error.
   - `ctest --test-dir build/<preset> --output-on-failure`.

A warning is a failure. Debug carries ASan/UBSan; release catches what only optimization exposes. Both must be clean.

## On any failure, warning, or error

1. Fix it. Smallest correct diff.
2. Rebuild and rerun the affected preset to confirm the fix holds.
3. Report what broke and what you changed, per fix, with the real failing output, not just counts.

## Report

Pass/fail per preset, then the fixes. Terse.
