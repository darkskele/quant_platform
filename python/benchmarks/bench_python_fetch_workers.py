#!/usr/bin/env python3
"""How the fetch path scales with pool workers.

No python callback in any run, so this is the fetch, unzip and parse path
alone. A stream may have as many fetches outstanding as it has slots, so the
concurrency a run can actually ask for is streams times slots, and the worker
count only matters below that. Five years, because the sweep needs more files
than concurrency or wall time is quantised into a couple of waves and the
result says more about rounding than about the path.
"""
from __future__ import annotations

import math

import harness

YEARS = 5
WORKER_COUNTS = [4, 8, 16, 32, 64]


def main():
    slots = harness.qpb.FILE_SLOTS
    rows = []
    streams = 0
    for workers in WORKER_COUNTS:
        backtest, _, phases, _, _ = harness.run(YEARS, workers, "no python")
        stats = backtest.fetch_stats()
        streams = backtest.stream_count()
        rows.append(
            (
                workers,
                phases["run"] / 1e9,
                stats.completed_ok,
                stats.bytes_fetched / 1e6,
                stats.completed_failed,
                stats.retries,
            )
        )

    ceiling = streams * slots
    harness.heading(f"fetch path by worker count  ({YEARS}y)")
    print(
        f"  {streams} streams x {slots} slots each, so {ceiling} fetches is all this"
        f" universe can ask for"
    )
    print()
    print(
        f"  {'workers':>8}{'conc':>6}{'waves':>7}{'run':>9}{'per fetch':>11}"
        f"{'files/s':>9}{'MB/s':>8}{'vs first':>10}{'failed':>8}{'retries':>9}"
    )

    base = rows[0][1]
    for workers, seconds, files, megabytes, failed, retries in rows:
        concurrency = min(workers, ceiling)
        waves = math.ceil(files / concurrency) if concurrency else 0
        per_fetch = seconds / waves if waves else 0.0
        idle = "*" if workers > ceiling else " "
        print(
            f"  {workers:>7}{idle}{concurrency:>6}{waves:>7}{seconds:>8.2f}s"
            f"{per_fetch * 1e3:>10.0f}ms{files / seconds:>9.1f}{megabytes / seconds:>8.2f}"
            f"{base / seconds:>9.2f}x{failed:>8,}{retries:>9,}"
        )

    print()
    print("  per fetch is the run divided by the waves it takes, which is the only")
    print("  honest latency here since a run is whole waves and nothing finer.")
    print("  A flat per fetch column means the path is scaling and the worker count")
    print("  is the only limit. It rising, or failed and retries leaving zero, is")
    print("  the bucket pushing back and the point past which workers stop paying.")
    print("  A starred row wants more workers than the universe can use.")


if __name__ == "__main__":
    main()
