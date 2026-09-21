---
name: amend
description: Make everything straight before a commit. Runs test, checks the repo structure and the doc/comment standard hold, then audits the changed hot-path code for inefficiencies and needless copies. Fixes the mechanical violations, reports the performance findings. Explicit invocation only (/amend), never proactively.
---

Explicit invocation only (`/amend`).

Scope: the current change (`git status --short` + `git diff --staged --name-only`) for the file-level reviews; a quick full-tree scan for the structure invariant. Never touch the index.

## 1. Tests and benches

- Run `test`. Every launch green, no warnings or sanitizer reports, and the bench summary with any confirmed regression. Fix anything that isn't green.

## 2. Repo structure holds

- Every directory owning a concept or a composed sub-module has a `README.md` + `DECISIONS.md` at its mirrored `docs/` path.
- A component under `include/` is mirrored in `cmake/`, `tests/`, `benchmarks/`, and `docs/`.
- A leaf variation is a bullet in its parent, not its own doc.

Flag any dir missing its doc, or any of the four trees out of sync.

## 3. Docs and comments follow the writing skill

Load the `writing` skill and check the changed files against it:

- READMEs: brief, diagram (stops at the seam, names no specific variation), components vs variations split, module-local milestones. The level rule: no reference up, sideways, or to a specific impl a level down.
- DECISIONS: module-local numbered one-liners.
- Comments: at most 4 lines, self-contained, Doxygen brief on public seams, no banned patterns.
- Soft-wrap, no em dashes anywhere.
- `clang-format` clean: `clang-format --dry-run --Werror` over each changed `.hpp`/`.cpp`.

Fix violations.

## 4. Performance audit (changed hot-path files)

Read the changed hot-path source. Flag anything working against the latency and performance goals:

- heap allocation, copies, or indirection in the per-event or per-tick path, as opposed to one-time setup;
- a missing move, `span`/`string_view`, or `constexpr` where it applies;
- cache-hostile layout, or false sharing on state touched from more than one thread;
- a non-obvious hot-path trade-off with no inline WHY.

Report these as findings with `file:line`. Don't rewrite: a performance change is the user's call.

## Report

Terse. Per section: what passed, what you fixed, what you are flagging. No prose summary.
