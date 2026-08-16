---
name: test
description: Build and run this repo's GoogleTest suites directly via Bash — the VS Code cppdbg/gdb debug-launch path is currently broken/flaky under WSL2 in this window, so this is the reliable way to actually execute tests. Targeted-by-default (only what current changes affect), or "all", or a specific lib/target. ONLY invoked when the user explicitly asks (/test or "run the tests") — never proactively, per CLAUDE.md's Build workflow rule.
---

Explicit invocation only (`/test`, `/test all`, `/test <lib>`) — never on
own judgment just because code changed. Run via Bash directly (`cmake
--build` then `ctest`), never a VS Code task or gdb — plain execution is
what's confirmed working here.

## Targets (update this table when a lib/test target is added or renamed)

| Lib | Test target(s) | Depends on (transitively re-test if this changed) |
|---|---|---|
| `libs/core` | `qp_core_tests` | — |
| `libs/data_source/source` | `qp_source_tests`, `qp_protocol_integration_tests`, `qp_venue_tests` | core |
| `libs/data_source/sink` | `qp_sink_tests`, `qp_sink_integration_tests` | core |
| `apps/collector` | `qp_collector_integration_tests` | core, marketdata, record |

`libs/data_source/source`'s row covers its nested villages too (`venue`,
`protocol` — see `libs/data_source/source/docs/DESIGN.md`): a change anywhere
under `libs/data_source/source/**` maps to this row by path prefix, no separate
village rows needed. `qp_venue_tests` stays individually invocable by exact
target name (`/test qp_venue_tests`) when only that village changed. For the
build step, `cmake --build build/debug --target qp_prod_streamer_tests -j`
is equivalent to listing all three target names — either works.

Every target above is registered with CTest as `add_test(NAME <target>
COMMAND <target>)` in its lib's `CMakeLists.txt` — the test name equals the
CMake target name, so `ctest -R` can filter on the same names as this table.

## Selecting what to run

- **No argument (default): targeted.** Union of `git status --short` and
  `git diff --staged --name-only`. Map changed paths to the table by
  `libs/<name>/` (nested villages map to their town's row), then pull in
  anything transitively depending on a changed lib (`libs/core/*` changed →
  rerun everything). Doesn't map cleanly — top-level files, several unrelated
  libs, nothing changed — fall back to **all**, don't guess narrow.
- **`all`**: every target in the table, ignore git state.
- **A lib or target name** (`/test record`, `/test qp_sink_tests`): just
  that one, no diff inspection.

## Steps

1. `cmake --preset debug` — configure; a cheap no-op if nothing changed.
2. `cmake --build build/debug --target <selected targets> -j` — pass the
   exact selected target names, space-separated. Don't fall back to a bare
   `cmake --build build/debug -j` for a targeted run — that silently builds
   the whole tree regardless of what was actually selected.
3. `ctest --test-dir build/debug --output-on-failure -R '<target1|target2|...>'`
   for a targeted/lib-specific run; omit `-R` entirely for `all`.
4. Report tersely: pass/fail counts per target run. On failure, include the
   real failing-assertion output — not just "N failed," show what broke.
