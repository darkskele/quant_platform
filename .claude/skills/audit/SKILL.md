---
name: audit
description: Multi-faceted review of staged changes — design consistency (DESIGN.md/STATUS.md/DECISIONS.md drift at the directory level each staged file belongs to, re-run tests/benchmarks), hot-path performance (cache-awareness, modern C++, inline trade-off comments) on staged hot-path files, and style/lint (clang-format clean, comment-convention compliance) on staged files. Token-conscious — reads only the staged diff plus the owning directory's docs, not the whole tree. ONLY invoked explicitly (/audit) — never proactively.
---

Explicit invocation only (`/audit`) — never on own judgment just because
code changed.

## Scope: derived from staged changes, not a typed-in path

1. `git diff --cached --name-only`. Nothing staged → say so and stop; don't
   fall back to unstaged changes or guess a directory (the user works out
   of staged changes deliberately — same reasoning as never touching the
   index without being asked).
2. For each staged file, walk up from its containing directory until you
   find one with a `docs/DESIGN.md` (per the city/town/village hierarchy,
   `docs/repo-layout.md`) — that's the file's **owning directory**. Leaf
   villages (`venue/`, `protocol/`) have no `DESIGN.md` of their own; their
   files own up to the town. Root `docs/DESIGN.md` is the ceiling — every
   path resolves to *something*.
3. Dedupe into a set of owning directories. Run the full three-facet pass
   (below) once per owning directory, each scoped to *that directory's*
   staged files. Multiple owning directories in one invocation → one report
   section per directory, in the report format at the end.

## Facets

Three, one pass per owning directory: **A. design consistency** (whole-
directory docs, cross-checked against what's staged), **B. hot-path
performance** (staged files only), **C. style & comment lint** (staged
files only). Read-only throughout — never edits `docs/STATUS.md`,
`docs/DECISIONS.md`, or code. A human applies whatever the report turns up.
(An automated write-back-on-review loop was considered and rejected earlier
in this project's history as exactly the per-commit cascading automation
that doesn't fit a solo-dev, local, explicit-invocation workflow.)

### Facet A — design consistency

1. **Read** the owning directory's `docs/DESIGN.md` (goals + success
   metrics — a struck-through goal is claimed done), `docs/STATUS.md`
   (goal-title list + last-proof section — a goal is title-only if done,
   expanded with a description if not), `docs/DECISIONS.md` if it exists.
   Don't read anything outside the owning directory unless a goal
   explicitly points elsewhere.

2. **Re-run, don't trust.** Invoke `test`, and `bench` if `DESIGN.md` names
   one, scoped to this directory. Compare against `STATUS.md`'s last-proof
   claim. Flag drift either direction — a new failure, or `STATUS.md`
   under-claiming what's actually passing. Also flag `DESIGN.md`/`STATUS.md`
   goal-state drift itself: a goal struck through in one but not
   correspondingly title-only/expanded in the other is a finding regardless
   of what the tests say.

3. **Doc-structure check.** Composition-level directories (wire together
   concrete types from more than one lib/village — today, `apps/*`) need
   the diagram → generic-components list → source-file rundown, in that
   order, *before* the goals section (see `apps/collector/docs/DESIGN.md`
   for the shape). Leaf/pure-logic directories don't need this — flag its
   absence only where the directory is actually a composition point.
   Separately: if the staged diff touches source files directly (not just
   docs), confirm `DESIGN.md`'s generic-components / rundown section names
   those files' components — new/changed source with no matching doc
   mention is a finding. Implementation details belong in the code, not the
   doc — don't flag a design doc for *lacking* implementation detail, only
   for lacking the component-level description.

4. **Implementation-vs-design scan.** For each staged file, check whether
   `DESIGN.md`'s description of the component it belongs to still matches
   structurally (what it depends on, what it owns, the shape of its
   interface) — not line-by-line, just "does the doc still describe what's
   there." Where it's diverged:
   - Check `DECISIONS.md` for an entry that justifies the divergence. Found
     one → not a finding (the doc's staleness is the decision log's job to
     explain, not `DESIGN.md`'s). Report which decision covers it.
     Not found → a finding. Say which side looks wrong: if the staged
     change is clearly deliberate/better than what's documented, recommend
     updating `DESIGN.md` (and note it wants a `DECISIONS.md` entry); if it
     looks like accidental drift, recommend fixing the code back to match
     the design. Don't guess silently — say which you think it is and why,
     but leave the call to the human.

5. **Decision-drift check — bounded.** Only for `DECISIONS.md` entries with
   no mention in `STATUS.md`'s last "as of" note (plausibly newer than the
   last proof) — otherwise skip, don't re-audit full history every call.
   Read the code each such entry is about; a decision that's silently
   stopped matching it (not struck through, not superseded, just quietly
   wrong) is a finding.

### Facet B — hot-path performance

Scope: staged files that are also components `DESIGN.md` marks hot-path
(has a bench-table row, or a goal explicitly calling out a benchmark) —
skip staged files that aren't hot-path, and skip unstaged hot-path files
that weren't touched.

6. **Hot-path/bench cross-check.** Each staged hot-path component needs a
   real, currently-building row in the `bench` skill's table — no match, or
   a benchmark that no longer builds, is a finding.

7. **Read the staged hot-path source.** For each such file, check for:
   - unnecessary heap allocation, copies, or indirection in the loop that
     actually runs per-message/per-tick (vs. one-time setup);
   - cache-conscious layout where it matters (contiguous storage over
     pointer-chasing, hot fields grouped, false-sharing on shared state
     touched from multiple threads — e.g. the SPSC queue's producer/consumer
     indices);
   - modern-C++ idioms available and not used without reason (`concept`s
     over SFINAE/CRTP for compile-time seams, `string_view`/`span` over
     copying, `constexpr`/`consteval` where a value is genuinely
     compile-time, move semantics on the hot path);
   - a non-obvious performance trade-off (why a lock-free structure here,
     why this branch is ordered this way, why a copy was accepted) that
     has **no inline comment near the code explaining it**. Missing
     rationale for a non-obvious hot-path choice is a finding — not "this
     could be faster," but "this choice needs a WHY and doesn't have one."

   This is a review, not a rewrite: report findings, don't restructure the
   code. Don't flag micro-optimizations with no evidence they matter here —
   `bench`'s numbers are the evidence; use them if a finding needs backing.

### Facet C — style & comment lint

Scope: staged `.hpp`/`.cpp` files only — not the rest of the owning
directory.

8. **clang-format clean.** Run `clang-format --dry-run --Werror` (no `-i`,
   this is read-only) over each staged `.hpp`/`.cpp`. Any file that isn't
   clean is a finding — name the file, don't paste the diff.

9. **Comment-convention check**, per `CLAUDE.md`'s comment standard:
   - Doxygen tags (`///`, `@param`, `@return`, `@pre`, `@warning`) belong on
     a declaration only where there's a non-obvious WHY to say — a
     declaration-level comment that just restates the function name/params,
     or a comment block on every function regardless of whether it says
     anything, is a finding (over-commenting, not under).
   - Implementation detail/trade-off explanation belongs **inline in the
     body**, not stacked into the declaration comment — a long or wordy
     declaration-level comment carrying implementation detail is a finding;
     the fix is "move the WHY inline, keep the declaration comment short or
     absent," not "delete the explanation."
   - This is new-code convention, not a retrofit mandate — don't flag
     pre-existing (unstaged) code the CLAUDE.md rule predates.

## Report

Terse, bulleted, tagged by owning directory then facet. One line per goal
for facet A's core check, then a short findings list per facet for
everything else:

```
## <owning directory>
G<n> — pass|fail|unproven|drift: <one-clause reason>
...
[A] <finding — file/goal ref, one clause, DESIGN-side or code-side call if applicable>
[B] <finding — file:line, one clause>
[C] <finding — file[:line], one clause>
```

Repeat the block per owning directory. No prose summary. Omit a facet's
section entirely if it produced nothing; omit a directory's block entirely
if all three facets produced nothing.
