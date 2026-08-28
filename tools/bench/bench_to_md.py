#!/usr/bin/env python3
"""Render a Google Benchmark JSON report (--benchmark_out_format=json) as a
Markdown table, grouped into one section per class under benchmark, e.g.
"### SpscQueue". Expects --benchmark_repetitions=N (+ default aggregates)
so the "variance" columns (mean/median/stddev/cv rows) are Google
Benchmark's own repetition statistics, not a hand-rolled stat — see
docs/decisions.md on why we stuck to stock Google Benchmark instead of
per-iteration percentile sampling (WSL2 scheduling noise dominates the
tail at this level; also plain wall-clock instrumentation overhead swamps
sub-100ns ops).

Grouping relies on the repo-wide benchmark naming convention
(benchmarks/README.md): every benchmark function is named
`BM_<Class>_<Description>`, so the class is mechanically the text between
`BM_` and the first following underscore. There is no other place to get
this from — Google Benchmark's JSON has no source-file or category field,
only the name — so a benchmark that doesn't follow the convention (no
underscore after the class) becomes its own single-entry "Other" group
rather than silently crashing or getting merged into an unrelated one.
"""

import argparse
import json
import re
import sys

_NAME_RE = re.compile(r"^BM_([A-Za-z0-9]+)_")

_FIXED_KEYS = {
    "name", "run_name", "run_type", "repetitions", "repetition_index",
    "threads", "iterations", "real_time", "cpu_time", "time_unit",
    "aggregate_name", "aggregate_unit", "family_index", "per_family_instance_index",
}


def _fmt(value):
    if isinstance(value, float):
        return f"{value:,.2f}"
    return str(value)


def _fmt_time(value, case):
    # cv rows carry a fraction under aggregate_unit "percentage", not a
    # duration — time_unit is still "ns" on those rows regardless, so it
    # has to be checked explicitly rather than trusted at face value.
    if case.get("aggregate_unit") == "percentage":
        return f"{value * 100:.2f} %"
    return f"{_fmt(value)} {case.get('time_unit', '')}"


def _extract_class(label: str) -> str:
    m = _NAME_RE.match(label)
    return m.group(1) if m else "Other"


def render(report: dict) -> str:
    cases = report.get("benchmarks", [])

    # One row per benchmark: pivot mean/median/stddev/cv (rows sharing a
    # family_index, distinguished only by aggregate_name) into columns
    # instead. Grouped by family_index, NOT run_name/name — two unrelated
    # benchmarks in the same binary (e.g. different files each defining
    # their own BM_PushInt) share a display name but never a family_index,
    # and grouping by name alone silently merges them into one row,
    # dropping the rest (hit for real: bench_spsc_queue.cpp,
    # bench_spmc_ring.cpp, and bench_mpsc_queue.cpp all had a BM_PushInt
    # before they were renamed unique — the collision cost two of the
    # three rows silently before this was keyed correctly). Requires
    # --benchmark_repetitions=N; a plain single-shot run (no
    # aggregate_name) still gets one row, just with median/stddev/cv blank.
    groups: dict[object, dict[str, dict]] = {}
    order: list[object] = []
    labels: dict[object, str] = {}
    for case in cases:
        key = case.get("family_index", case.get("run_name", case.get("name", "")))
        if key not in groups:
            groups[key] = {}
            order.append(key)
            labels[key] = case.get("run_name", case.get("name", ""))
        groups[key][case.get("aggregate_name", "value")] = case

    seen_labels: dict[str, object] = {}
    for key in order:
        label = labels[key]
        if label in seen_labels and seen_labels[label] != key:
            print(f"warning: duplicate benchmark name '{label}' (distinct family_index) — "
                  f"rows are correctly separate but indistinguishable by name in the table",
                  file=sys.stderr)
        seen_labels[label] = key

    # Group by class (benchmarks/README.md's BM_<Class>_<Description>
    # convention), preserving first-seen order of both classes and rows
    # within each class — stable output, no alphabetical reshuffling.
    classes: dict[str, list] = {}
    class_order: list[str] = []
    for key in order:
        cls = _extract_class(labels[key])
        if cls not in classes:
            classes[cls] = []
            class_order.append(cls)
        classes[cls].append(key)

    base_headers = ["Benchmark", "Reps", "Threads", "Time", "CPU", "Median", "StdDev", "CV"]

    sections = []
    for cls in class_order:
        # Counter columns (e.g. items_per_second) scoped to what this
        # class's own rows actually report — a class that never sets one
        # doesn't carry an always-blank column just because some other,
        # unrelated class in the same report does.
        counter_keys = []
        for key in classes[cls]:
            mean_case = groups[key].get("mean", groups[key].get("value"))
            if not mean_case:
                continue
            for k in mean_case:
                if k not in _FIXED_KEYS and k not in counter_keys:
                    counter_keys.append(k)

        headers = [*base_headers, *counter_keys]
        lines = [
            f"### {cls}", "",
            "| " + " | ".join(headers) + " |",
            "| " + " | ".join(["---"] * len(headers)) + " |",
        ]
        for key in classes[cls]:
            stats = groups[key]
            base = stats.get("mean", stats.get("value"))
            median = stats.get("median")
            stddev = stats.get("stddev")
            cv = stats.get("cv")
            row = [
                labels[key],
                str(base.get("iterations", "")) if base else "",
                str(base.get("threads", 1)) if base else "",
                _fmt_time(base.get("real_time"), base) if base else "",
                _fmt_time(base.get("cpu_time"), base) if base else "",
                _fmt_time(median.get("real_time"), median) if median else "",
                _fmt_time(stddev.get("real_time"), stddev) if stddev else "",
                _fmt_time(cv.get("real_time"), cv) if cv else "",
            ]
            row += [_fmt(base[k]) if base and k in base else "" for k in counter_keys]
            lines.append("| " + " | ".join(row) + " |")
        sections.append("\n".join(lines))
    return "\n\n".join(sections) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("json_path", help="Google Benchmark JSON report")
    parser.add_argument("-o", "--out", help="Markdown file to write (default: stdout)")
    parser.add_argument("--title", default=None, help="Optional heading to prepend")
    args = parser.parse_args()

    with open(args.json_path) as f:
        report = json.load(f)

    body = render(report)
    if args.title:
        body = f"## {args.title}\n\n" + body

    if args.out:
        with open(args.out, "w") as f:
            f.write(body)
    else:
        sys.stdout.write(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
