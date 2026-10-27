#!/usr/bin/env python3
"""The base and hard cases, and capacity, as large figures per backend.

    python tools/netbench/plots.py benchmarks/network/results/results.json -o benchmarks/network/results/plots

<direction>_<backend>_<case>.png plots the direction's latency against percentile, the median of the valid
repetitions, with p99 marked and quoted per variant. <direction>_<backend>_throughput.png and
_latency.png plot what arrived, and its p99, against the load offered, with losses and steps the
sender could not send marked. Needs matplotlib.
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
CAPACITY_PREFIX = "load_"

# The latency each direction is judged by, and how to name it.
LATENCY = {"receive": ("send_to_user", "send to user latency"),
           "send": ("send_to_kernel", "write to peer kernel latency")}


def latency_of(run):
    return LATENCY[run.get("direction", "receive")]

# A variant takes the colour at its place in the sweep's variant list.
COLOURS = ["#2a78d6", "#eb6834", "#1baf7a"]
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
    sent = run["sender"]["count"] + run["sender"]["warmup"]
    return run["receiver"]["counters"]["gaps"] / sent if sent else 0


def style(ax):
    ax.set_facecolor(SURFACE)
    ax.grid(axis="both", which="major", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=10)


def load_steps(runs):
    """Per variant, each load step's offered rate, p99, sent and received rates and loss."""
    by_variant = defaultdict(lambda: defaultdict(list))
    for r in runs:
        by_variant[r["variant"]][r["offered"]].append(r)
    out = {}
    for variant, steps in by_variant.items():
        offered = sorted(steps)

        def med(f):
            return [median(f(r) for r in steps[o]) for o in offered]

        sent = med(lambda r: (r["sender"]["count"] + r["sender"]["warmup"]) / r["sender"]["elapsed_ns"]
                   * 1e9)
        out[variant] = {
            "offered": offered,
            "p99": med(lambda r: r["receiver"]["stages"][latency_of(r)[0]]["p99"] / 1000),
            "received": med(lambda r: r["receiver"]["counters"]["messages"]
                            / r["receiver"]["span_ns"] * 1e9 if r["receiver"].get("span_ns")
                            else None),
            "lost": med(loss),
            # A step the sender could not send at says nothing about the receiver.
            "short": [s < 0.95 * o for s, o in zip(sent, offered)],
        }
    return out


def mark(ax, xs, ys, shorts, colour):
    """Filled where the step is a receiver result, hollow where the sender fell short. The surface
    coloured edge rings the marker where lines cross."""
    for x, y, short in zip(xs, ys, shorts):
        if y is not None:
            ax.plot(x, y, "o", markersize=9, markeredgewidth=2,
                    color=SURFACE if short else colour,
                    markeredgecolor=colour if short else SURFACE)


def load_figure(backend, runs, variants, title, ylabel, plot, out):
    """One panel against offered load, a line per variant. plot draws a variant's line and returns
    its legend label."""
    fig, ax = plt.subplots(figsize=(12, 6.5), facecolor=SURFACE)
    style(ax)
    handles, labels = [], []
    steps = load_steps(runs)
    for variant, colour in zip(variants, COLOURS):
        if variant in steps:
            line, label = plot(ax, steps[variant], colour)
            handles.append(line)
            labels.append(f"{variant}   {label}")
    handles.append(plt.Line2D([], [], color=TEXT_SECONDARY, marker="o", markersize=9,
                              markerfacecolor=SURFACE, linestyle="none"))
    labels.append("sender sent under 95% of the offered load")
    ax.legend(handles, labels, loc="upper left", frameon=False, fontsize=11, labelcolor=TEXT,
              title="variant", title_fontsize=10, alignment="left")
    ax.set_xlabel("offered load, thousand msg/s", fontsize=10, color=TEXT_SECONDARY)
    ax.set_ylabel(ylabel, fontsize=10, color=TEXT_SECONDARY)

    first = runs[0]
    peers = f"{first['peers']} peers, " if first.get("peers", 1) > 1 else ""
    fig.suptitle(f"{backend}, {title}", x=0.01, ha="left", fontsize=14, color=TEXT)
    fig.text(0.01, 0.925, f"{peers}512 B messages at fixed rate steps   median of up to 3 runs "
             f"per step", fontsize=10, color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0, 1, 0.92))
    fig.savefig(out, dpi=120, facecolor=SURFACE)
    plt.close(fig)


def plot_throughput(ax, step, colour):
    k = [o / 1000 for o in step["offered"]]
    got = [r / 1000 if r is not None else None for r in step["received"]]
    # Every offered message taken lies on the diagonal. Below it the receiver could not keep up.
    ax.plot(k, k, color=TEXT_SECONDARY, linewidth=1, linestyle=(0, (4, 4)))
    (line,) = ax.plot(k, got, color=colour, linewidth=2)
    mark(ax, k, got, step["short"], colour)
    for x, y, l in zip(k, got, step["lost"]):
        if l and y is not None:
            ax.annotate(f"lost {l:.2%}", (x, y), xytext=(0, -18), textcoords="offset points",
                        ha="center", fontsize=10, color=TEXT)
    ax.set_ylim(bottom=0)
    best = max((y for y in got if y is not None), default=None)
    return line, f"most received {best:,.0f}k msg/s" if best else ""


def plot_latency(ax, step, colour):
    k = [o / 1000 for o in step["offered"]]
    ax.set_yscale("log")
    (line,) = ax.plot(k, step["p99"], color=colour, linewidth=2)
    mark(ax, k, step["p99"], step["short"], colour)
    for x, y, l in zip(k, step["p99"], step["lost"]):
        if l:
            ax.annotate(f"lost {l:.2%}", (x, y), xytext=(0, 12), textcoords="offset points",
                        ha="center", fontsize=10, color=TEXT)
    ax.yaxis.set_major_locator(LogLocator(subs=(1, 2, 5)))
    ax.yaxis.set_major_formatter(FuncFormatter(fmt_us))
    ax.yaxis.set_minor_formatter(NullFormatter())
    clean = [x for x, l, sh in zip(k, step["lost"], step["short"]) if not l and not sh]
    return line, f"clean up to {clean[-1]:g}k msg/s" if clean else ""


def case_figure(backend, case, workload, by_variant, variants, out):
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
    for variant, colour in zip(variants, COLOURS):
        runs = by_variant.get(variant, [])
        if not runs:
            continue
        ys = [median(percentile(r["receiver"]["stages"][latency_of(r)[0]], p) for r in runs)
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
        label = f"{variant}   p50 {quote_us(us[0])}   p99 {quote_us(p99)}"
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
    ax.set_ylabel(latency_of(next(iter(by_variant.values()))[0])[1], fontsize=10,
                  color=TEXT_SECONDARY)
    ax.legend(handles, labels, loc="upper left", frameon=False, fontsize=11, labelcolor=TEXT,
              title="variant", title_fontsize=10, alignment="left")

    reps = max((len(rs) for rs in by_variant.values()), default=0)
    first = next(iter(by_variant.values()))[0]
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

    results = json.loads(pathlib.Path(args.results).read_text())
    variants = results["variants"]
    runs = [r for r in results["runs"] if not r["void"]]
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for stale in out.glob("*.png"):
        stale.unlink()
    grouped = defaultdict(lambda: defaultdict(list))
    for r in runs:
        grouped[(r.get("direction", "receive"), r["backend"], r["workload"])][
            r["variant"]].append(r)
    for direction, backend in sorted({(d, b) for d, b, _ in grouped}):
        name = f"{direction}_{backend}"
        for case, workload in CASES.items():
            by_variant = grouped.get((direction, backend, workload))
            if by_variant:
                case_figure(backend, case, workload, by_variant, variants,
                            out / f"{name}_{case}.png")
        load = [r for r in runs if r.get("direction", "receive") == direction
                and r["backend"] == backend and r.get("offered")
                and r["workload"].startswith(CAPACITY_PREFIX)]
        if load:
            load_figure(backend, load, variants, "capacity: what arrived",
                        "received, thousand msg/s", plot_throughput, out / f"{name}_throughput.png")
            load_figure(backend, load, variants, "capacity: latency under load",
                        f"{LATENCY[direction][1]}, p99", plot_latency, out / f"{name}_latency.png")


if __name__ == "__main__":
    main()
