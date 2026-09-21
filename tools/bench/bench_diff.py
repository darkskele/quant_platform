#!/usr/bin/env python3
"""Diff two Google Benchmark JSON reports (--benchmark_out_format=json) and
render a Markdown table of real_time/cpu_time deltas, matched by benchmark
name (run_name, falling back to name — stable across runs, unlike
family_index which is reassigned fresh per report).

A delta only counts as a real diff, not WSL2 noise, if it passes three tests
on real_time OR cpu_time. First the whole suite's shift is divided out: the
median current/baseline ratio across every shared benchmark is machine state,
not code, since one change cannot move every benchmark at once. Then the
corrected delta has to clear BOTH a relative and an absolute floor (default 5%
and 1.5 ns), since relative alone misflags sub-nanosecond benchmarks and
absolute alone misflags large ones. Then it has to sit outside sigma (default 2)
standard deviations of the two runs' combined repetition spread, so a benchmark
that jitters widely has to move further to count. A report without repetitions
has no spread, and only the floors apply.
Rows clearing neither are counted, not shown — the table is the thing
worth reading before a commit, not a full noise-included dump (that's what
the tracked bench_results.json is for). Rows sorted worst regression
first by real_time delta (wall-clock — what a caller actually feels).

Only diffs real_time/cpu_time — counters like items_per_second aren't
diffed. A benchmark present in only one report is listed separately
(added/removed), not silently dropped.
"""

import argparse
import json
import math
import statistics
import sys

_UNIT_TO_NS = {"ns": 1, "us": 1e3, "ms": 1e6, "s": 1e9}


def _spread(report: dict) -> dict[str, dict]:
    """The stddev aggregate per benchmark, where repetitions produced one."""
    return {case.get("run_name", case.get("name", "")): case
            for case in report.get("benchmarks", [])
            if case.get("aggregate_name") == "stddev"}


def _index(report: dict) -> dict[str, dict]:
    # Mirrors bench_to_md.py's grouping: pick the "mean" aggregate case if
    # repetitions were used, else the lone plain case (aggregate_name
    # absent -> key "value"). stddev/median/cv rows are skipped here, this
    # diff only cares about the central value.
    groups: dict[str, dict] = {}
    for case in report.get("benchmarks", []):
        label = case.get("run_name", case.get("name", ""))
        agg   = case.get("aggregate_name", "value")
        if agg not in ("mean", "value"):
            continue
        if label not in groups or agg == "mean":
            groups[label] = case
    return groups


def _ns(case: dict, field: str) -> float:
    return case[field] * _UNIT_TO_NS.get(case.get("time_unit", "ns"), 1)


def _fmt_ns(ns: float) -> str:
    return f"{ns:,.2f} ns"


def _fmt_pct(pct: float) -> str:
    sign = "+" if pct >= 0 else ""
    return f"{sign}{pct:.1f}%"


def _pct(base: float, cur: float) -> float:
    return ((cur - base) / base * 100) if base else 0.0


def _clears(base: float, cur: float, spread: float, threshold_pct: float, threshold_ns: float,
            sigma: float) -> bool:
    delta = abs(cur - base)
    return (delta >= threshold_ns and abs(_pct(base, cur)) >= threshold_pct
            and delta > sigma * spread)


def _drift(base_index: dict, cur_index: dict, shared: list[str], field: str) -> float:
    """Median current/baseline ratio, the part of every delta that is the machine."""
    ratios = [_ns(cur_index[label], field) / _ns(base_index[label], field)
              for label in shared if _ns(base_index[label], field) > 0]
    return statistics.median(ratios) if ratios else 1.0


def _sd(spread: dict, label: str, field: str) -> float:
    case = spread.get(label)
    return _ns(case, field) if case else 0.0


def diff(baseline: dict, current: dict, threshold_pct: float, threshold_ns: float,
         sigma: float = 2.0) -> str:
    base_index, base_spread = _index(baseline), _spread(baseline)
    cur_index, cur_spread   = _index(current), _spread(current)

    shared  = [label for label in cur_index if label in base_index]
    added   = [label for label in cur_index if label not in base_index]
    removed = [label for label in base_index if label not in cur_index]

    drift_real = _drift(base_index, cur_index, shared, "real_time")
    drift_cpu  = _drift(base_index, cur_index, shared, "cpu_time")

    rows = []
    noise = 0
    for label in shared:
        base, cur = base_index[label], cur_index[label]
        base_real, cur_real = _ns(base, "real_time"), _ns(cur, "real_time") / drift_real
        base_cpu, cur_cpu   = _ns(base, "cpu_time"), _ns(cur, "cpu_time") / drift_cpu
        spread_real = math.hypot(_sd(base_spread, label, "real_time"),
                                 _sd(cur_spread, label, "real_time") / drift_real)
        spread_cpu  = math.hypot(_sd(base_spread, label, "cpu_time"),
                                 _sd(cur_spread, label, "cpu_time") / drift_cpu)
        if not (_clears(base_real, cur_real, spread_real, threshold_pct, threshold_ns, sigma) or
                _clears(base_cpu, cur_cpu, spread_cpu, threshold_pct, threshold_ns, sigma)):
            noise += 1
            continue
        rows.append((label, base_real, cur_real, base_cpu, cur_cpu))
    rows.sort(key=lambda r: _pct(r[1], r[2]), reverse=True)

    lines = ["## Benchmark diff (current vs. HEAD)", "",
             f"Suite-wide drift x{drift_real:.2f} real, x{drift_cpu:.2f} cpu (median "
             f"current/baseline), divided out below. Current columns are corrected.", ""]
    if rows:
        headers = ["Benchmark", "Baseline", "Current", "Δ Time", "Baseline CPU", "Current CPU", "Δ CPU"]
        lines += ["| " + " | ".join(headers) + " |", "| " + " | ".join(["---"] * len(headers)) + " |"]
        for label, base_real, cur_real, base_cpu, cur_cpu in rows:
            lines.append(
                f"| {label} | {_fmt_ns(base_real)} | {_fmt_ns(cur_real)} | {_fmt_pct(_pct(base_real, cur_real))} | "
                f"{_fmt_ns(base_cpu)} | {_fmt_ns(cur_cpu)} | {_fmt_pct(_pct(base_cpu, cur_cpu))} |"
            )
    else:
        lines.append("*No benchmarks changed beyond the noise floor.*")

    lines += ["", f"{noise} benchmark(s) unchanged (within noise: <{threshold_ns:g} ns, "
                  f"<{threshold_pct:g}%, or inside {sigma:g} sigma of the runs' spread)."]

    if added:
        lines += ["", f"**Added** ({len(added)}): " + ", ".join(sorted(added))]
    if removed:
        lines += ["", f"**Removed** ({len(removed)}): " + ", ".join(sorted(removed))]

    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline_json", help="Google Benchmark JSON report to compare against")
    parser.add_argument("current_json", help="Google Benchmark JSON report just produced")
    parser.add_argument("--threshold-pct", type=float, default=5.0,
                         help="relative noise floor, %% (default 5.0)")
    parser.add_argument("--threshold-ns", type=float, default=1.5,
                         help="absolute noise floor, ns (default 1.5)")
    parser.add_argument("--sigma", type=float, default=2.0,
                         help="combined stddevs a delta must exceed (default 2.0)")
    parser.add_argument("-o", "--out", help="Markdown file to write (default: stdout)")
    args = parser.parse_args()

    with open(args.baseline_json) as f:
        baseline = json.load(f)
    with open(args.current_json) as f:
        current = json.load(f)

    body = diff(baseline, current, args.threshold_pct, args.threshold_ns, args.sigma)
    if args.out:
        with open(args.out, "w") as f:
            f.write(body)
    else:
        sys.stdout.write(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
