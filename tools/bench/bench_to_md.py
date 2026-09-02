#!/usr/bin/env python3
"""Render a Google Benchmark JSON report as BENCHMARKS.md: a grouped summary
of every benchmark, then (when a baseline is given) the diff against it.

The summary is one section per class under the repo naming convention
(benchmarks/README.md: `BM_<Class>_<Description>`, so the class is the text
between `BM_` and the first following underscore), one row per benchmark,
with Google Benchmark's own repetition statistics as columns
(mean/median/stddev/cv). A benchmark that doesn't follow the convention
becomes its own single-entry "Other" group rather than crashing.

The diff is delegated to bench_diff.py and appended under its own heading.
A missing or unparseable baseline degrades to a note, never a crash.
"""

import argparse
import json
import re
import sys

from bench_diff import diff

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
    # duration — time_unit is still "ns" on those rows regardless.
    if case.get("aggregate_unit") == "percentage":
        return f"{value * 100:.2f} %"
    return f"{_fmt(value)} {case.get('time_unit', '')}"


def _extract_class(label: str) -> str:
    m = _NAME_RE.match(label)
    return m.group(1) if m else "Other"


def render_summary(report: dict) -> str:
    cases = report.get("benchmarks", [])

    # One row per benchmark: pivot mean/median/stddev/cv (rows sharing a
    # family_index, distinguished only by aggregate_name) into columns.
    # Grouped by family_index, NOT run_name/name — two unrelated benchmarks
    # in the same binary can share a display name but never a family_index,
    # and grouping by name alone silently merges them into one row.
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
            print(f"warning: duplicate benchmark name '{label}' (distinct family_index)",
                  file=sys.stderr)
        seen_labels[label] = key

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
        # Counter columns scoped to what this class's own rows report.
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
    return "\n\n".join(sections)


def render_document(report: dict, baseline, threshold_pct: float, threshold_ns: float) -> str:
    parts = [
        "# Benchmarks",
        "",
        "_Generated by `qp_bench` — Google Benchmark, 5 repetitions, aggregates only. "
        "This is WSL2: trust deltas between runs, not absolute numbers._",
        "",
        "## Summary",
        "",
        render_summary(report),
        "",
        "---",
        "",
    ]
    if baseline is None:
        parts += [
            "## Benchmark diff (current vs. HEAD)",
            "",
            "*No baseline available — commit a `bench_results.json` at HEAD to enable the diff.*",
        ]
    else:
        parts.append(diff(baseline, report, threshold_pct, threshold_ns).rstrip())
    return "\n".join(parts) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("json_path", help="Google Benchmark JSON report just produced")
    parser.add_argument("--baseline", default=None, help="JSON report to diff against (HEAD's)")
    parser.add_argument("--threshold-pct", type=float, default=5.0)
    parser.add_argument("--threshold-ns", type=float, default=1.5)
    parser.add_argument("-o", "--out", help="Markdown file to write (default: stdout)")
    args = parser.parse_args()

    with open(args.json_path) as f:
        report = json.load(f)

    baseline = None
    if args.baseline:
        try:
            with open(args.baseline) as f:
                baseline = json.load(f)
        except (OSError, json.JSONDecodeError) as e:
            print(f"warning: baseline unreadable ({e}) — summary only, no diff", file=sys.stderr)

    body = render_document(report, baseline, args.threshold_pct, args.threshold_ns)
    if args.out:
        with open(args.out, "w") as f:
            f.write(body)
    else:
        sys.stdout.write(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
