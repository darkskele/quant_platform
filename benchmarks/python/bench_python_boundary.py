#!/usr/bin/env python3
"""Prices the python boundary against the work underneath it.

Runs the same files three times, asking python for progressively more each
time. The first tier never crosses the boundary at all, so the step to the
second is the cost of entering python per event and the step to the third is
the strategy itself. One year, because the deltas are per event and do not need
the long window the profiling bench uses.
"""
from __future__ import annotations

import harness

YEARS = 1
WORKERS = 32


def main():
    rows = []
    events = 0
    for name, _ in harness.TIERS:
        backtest, profile, phases, _, _ = harness.run(YEARS, WORKERS, name)
        events = max(events, sum(r.stats.rows_parsed for r in backtest.reports()))
        rows.append((name, phases["run"] / 1e9, sum(profile.callback_ns)))

    harness.heading(f"python boundary  ({YEARS}y, {WORKERS} workers, {events:,} events)")
    print(f"  {'tier':<18}{'run':>9}{'events/s':>13}{'vs no python':>15}{'per event':>12}")
    base = rows[0][1]
    for name, seconds, _ in rows:
        delta = seconds - base
        per_event = (delta / events * 1e9) if events else 0.0
        print(
            f"  {name:<18}{seconds:>8.2f}s{events / seconds:>13,.0f}"
            f"{delta:>14.2f}s{harness.fmt_ns(per_event):>12}"
        )

    print()
    for name, description in harness.TIERS:
        print(f"  {name:<18}{description}")

    print()
    print("  The run column includes the network, which is the same files every")
    print("  tier, so the deltas are the python cost and the absolutes are not.")


if __name__ == "__main__":
    main()
