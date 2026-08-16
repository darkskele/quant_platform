---
name: amend-doc
description: Update quant-platform's docs when a decision, architecture change, or roadmap shift happens, instead of leaving it only in conversation. Use whenever a choice gets made that future sessions need to know about.
---

Route the change to the ONE doc that owns it — don't touch the others:

| Owns | Doc |
|---|---|
| North star, success tiers, non-goals | `MISSION.md` |
| Seams, the Engine, the rules | `docs/architecture-principles.md` |
| Strategy families & sequencing | `docs/strategy.md` |
| Data sources, collector, storage | `docs/data.md` |
| Build/tooling/perf/VCS/secrets | `docs/environment.md` |
| Phases & milestones | `docs/roadmap.md` |
| Directory tree, module boundaries | `docs/repo-layout.md` |
| Why a decision was made | `docs/decisions.md` (append-only ADR log) |

Rules:
- Smallest diff that makes the doc accurate again. Edit in place — don't
  rewrite surrounding prose, don't restate context the doc already has.
- Style: terse, imperative, plain prose. State the fact; cut the runway.
  No em dashes, no "it's worth noting" / "in order to" filler, no AI-tell
  phrasing. One clause beats two sentences. This is a spec, not an essay.
- A genuine decision (a choice + the reason) also gets one D-numbered entry
  appended to `docs/decisions.md`, 1–3 lines, matching the existing D1–D9
  style. A status update (scaffold built, phase started) is not a decision —
  it doesn't need one.
- Never duplicate a fact across docs. If two docs would need the same edit,
  that fact is misfiled — put it in the one that owns it and cross-reference.
- After editing, report back in one line: what changed, which doc. Not a
  summary of the doc's contents.
