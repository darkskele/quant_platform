#!/usr/bin/env python3
"""The base and hard cases as one large figure per backend, each receive path in its own colour.

    python tools/netbench/plots.py benchmarks/network/results/results.json -o benchmarks/network/results/plots

<backend>_<case>.png plots send to user latency against percentile, the median of the valid
repetitions, with p99 marked and quoted per receive path. Needs matplotlib.
"""

import argparse
import json
import math
import pathlib
import statistics
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter, LogLocator, NullFormatter  # noqa: E402

CASES = {"base": "base_fixed_100k_512", "hard": "hard_poisson_200k_mixed"}

# Fixed order, so a receive path keeps its colour whichever ones a sweep ran.
NET_COLOURS = {"napi": "#2a78d6", "default": "#eb6834", "rps": "#1baf7a"}
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"

PERCENTILES = [50, 60, 70, 80, 90, 95, 97, 99, 99.5, 99.7, 99.9, 99.95, 99.99]
TICKS = [50, 90, 99, 99.9, 99.99]
MARKED = [50, 99]


def tail_x(p):
    """Percentile on a scale where each extra nine is one step."""
    return -math.log10(1 - p / 100)


def percentile(h, p):
    """As the rig's histogram reports it, the top of the bucket holding rank p, capped at max."""
    if not h["count"]:
        return None
    rank = max(1, math.ceil(p / 100 * h["count"]))
    seen = 0
    for _, hi, n in h["buckets"]:
        seen += n
        if seen >= rank:
            return min(hi, h["max"])
    return h["max"]


def median(values):
    values = [v for v in values if v is not None]
    return statistics.median(values) if values else None


def fmt_us(us, _=None):
    return f"{us:g} µs" if us < 1000 else f"{us / 1000:g} ms"


def quote_us(us):
    return f"{us:.2f} µs" if us < 1000 else f"{us / 1000:.2f} ms"


def loss(run):
    sent = run["peer"]["count"] + run["peer"]["warmup"]
    return run["receiver"]["counters"]["gaps"] / sent if sent else 0


def case_figure(backend, case, workload, by_net, out):
    fig, ax = plt.subplots(figsize=(12, 6.5), facecolor=SURFACE)
    ax.set_facecolor(SURFACE)
    ax.set_yscale("log")
    ax.grid(axis="both", which="major", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=10)

    xs = [tail_x(p) for p in PERCENTILES]
    handles, labels = [], []
    for net, colour in NET_COLOURS.items():
        runs = by_net.get(net, [])
        if not runs:
            continue
        ys = [median(percentile(r["receiver"]["stages"]["send_to_user"], p) for r in runs)
              for p in PERCENTILES]
        us = [y / 1000 if y is not None else None for y in ys]
        (line,) = ax.plot(xs, us, color=colour, linewidth=2)
        for p in MARKED:
            y = us[PERCENTILES.index(p)]
            if y is not None:
                # The surface coloured edge rings the marker where lines cross.
                ax.plot(tail_x(p), y, "o", color=colour, markersize=8,
                        markeredgecolor=SURFACE, markeredgewidth=2)
        p99 = us[PERCENTILES.index(99)]
        lost = median(loss(r) for r in runs)
        label = f"{net}   p50 {quote_us(us[0])}   p99 {quote_us(p99)}"
        if lost:
            label += f"   lost {lost:.2%}"
        handles.append(line)
        labels.append(label)

    ax.axvline(tail_x(99), color=TEXT_SECONDARY, linewidth=1, linestyle=(0, (4, 4)))
    ax.text(tail_x(99), 1.01, "p99", transform=ax.get_xaxis_transform(), ha="center",
            va="bottom", fontsize=10, color=TEXT_SECONDARY)
    ax.set_xticks([tail_x(p) for p in TICKS], [f"p{p:g}" for p in TICKS])
    ax.yaxis.set_major_locator(LogLocator(subs=(1, 2, 5)))
    ax.yaxis.set_major_formatter(FuncFormatter(fmt_us))
    ax.yaxis.set_minor_formatter(NullFormatter())
    ax.set_xlabel("percentile, each step one more nine", fontsize=10, color=TEXT_SECONDARY)
    ax.set_ylabel("send to user latency", fontsize=10, color=TEXT_SECONDARY)
    ax.legend(handles, labels, loc="upper left", frameon=False, fontsize=11, labelcolor=TEXT,
              title="receive path", title_fontsize=10, alignment="left")

    reps = max((len(rs) for rs in by_net.values()), default=0)
    first = next(iter(by_net.values()))[0]
    args = " ".join(first["peer_args"])
    if first.get("peers", 1) > 1:
        args = f"{first['peers']} peers, each {args}"
    fig.suptitle(f"{backend}, {case} case: {workload}", x=0.01, ha="left", fontsize=14,
                 color=TEXT)
    fig.text(0.01, 0.925, f"{args}   median of up to {reps} runs", fontsize=10,
             color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0, 1, 0.92))
    fig.savefig(out, dpi=120, facecolor=SURFACE)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("results")
    parser.add_argument("-o", "--out", required=True)
    args = parser.parse_args()

    runs = [r for r in json.loads(pathlib.Path(args.results).read_text())["runs"] if not r["void"]]
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for stale in out.glob("*.png"):
        stale.unlink()
    grouped = defaultdict(lambda: defaultdict(list))
    for r in runs:
        grouped[(r["backend"], r["workload"])][r.get("net", "default")].append(r)
    for backend in sorted({b for b, _ in grouped}):
        for case, workload in CASES.items():
            by_net = grouped.get((backend, workload))
            if by_net:
                case_figure(backend, case, workload, by_net, out / f"{backend}_{case}.png")


if __name__ == "__main__":
    main()
