#!/usr/bin/env python3
"""The sweep's results as one markdown page.

    python3 tools/netbench/to_md.py benchmarks/network/results/results.json --env benchmarks/network/results/env.json \
        -o benchmarks/network/results/RESULTS.md

A section per direction, receive then send, each with a table per backend and a row per
workload. Each figure is the median across a row's valid repetitions. Receive latency is the
peer's send to the bytes reaching the backend's user space. Send latency is the backend's write
to the bytes reaching the peer's kernel, and write is how long that write held the caller.
sent/s is what the sender sent, recv/s what the receiver took over its own first to last message.
"""

import argparse
import json
import pathlib
import statistics
from collections import defaultdict

PERCENTILES = ["p50", "p90", "p99", "p99.9", "max"]

# The latency each direction is judged by.
LATENCY = {"receive": "send_to_user", "send": "send_to_kernel"}


def fmt_ns(ns):
    if ns is None:
        return ""
    if ns < 1_000:
        return f"{ns:.0f} ns"
    if ns < 1_000_000:
        return f"{ns / 1_000:.2f} µs"
    return f"{ns / 1_000_000:.2f} ms"


def median(values):
    values = [v for v in values if v is not None]
    return statistics.median(values) if values else None


def stage(run, name, key):
    h = run["receiver"]["stages"][name]
    return h[key] if h["count"] else None


def sent_messages(run):
    return run["sender"]["count"] + run["sender"]["warmup"]


def sent_rate(run):
    """Messages per second the sender sent."""
    elapsed = run["sender"]["elapsed_ns"]
    return sent_messages(run) / elapsed * 1e9 if elapsed else None


def write_call(run, key):
    """How long the backend's writes held the caller, sending only."""
    h = run["sender"].get("write_call")
    return h[key] if h and h["count"] else None


def received_rate(run):
    """Recorded messages per second, over the receiver's own first to last."""
    span = run["receiver"].get("span_ns")
    return run["receiver"]["counters"]["messages"] / span * 1e9 if span else None


def mean_size(run):
    """The average message size the peer sent, from its size:weight list."""
    sizes = run["peer_args"][run["peer_args"].index("--sizes") + 1]
    pairs = [(int(a), int(b or 1)) for a, _, b in (item.partition(":") for item in sizes.split(","))]
    return sum(size * weight for size, weight in pairs) / sum(weight for _, weight in pairs)


def loss(run):
    sent = sent_messages(run)
    return run["receiver"]["counters"]["gaps"] / sent if sent else None


def table(runs, direction):
    """One row per workload, in sweep order. The receive path joins the name when there are
    several."""
    rows = defaultdict(list)
    for r in runs:
        rows[(r["workload"], r["net"])].append(r)
    several_nets = len({net for _, net in rows}) > 1
    extra = ["k→u p50", "k→u p99"] if direction == "receive" else ["write p50", "write p99"]
    head = ["Workload", "Valid", *PERCENTILES, *extra, "msg/read", "sent/s", "recv/s", "recv MB/s",
            "lost"]
    lines = ["| " + " | ".join(head) + " |", "|" + " --- |" * len(head)]
    for (workload, net), rs in rows.items():
        ok = [r for r in rs if not r["void"]]
        cells = [f"{workload} / {net}" if several_nets else workload, f"{len(ok)}/{len(rs)}"]
        cells += [fmt_ns(median(stage(r, LATENCY[direction], p) for r in ok)) for p in PERCENTILES]
        if direction == "receive":
            cells += [fmt_ns(median(stage(r, "kernel_to_user", p) for r in ok))
                      for p in ("p50", "p99")]
        else:
            cells += [fmt_ns(median(write_call(r, p) for r in ok)) for p in ("p50", "p99")]
        per_read = median(stage(r, "messages_per_read", "mean") for r in ok)
        cells.append(f"{per_read:.1f}" if per_read else "")
        sent = median(sent_rate(r) for r in ok)
        received = median(received_rate(r) for r in ok)
        lost = median(loss(r) for r in ok)
        mb = received * mean_size(rs[0]) / 1e6 if received else None
        cells += [f"{sent:,.0f}" if sent else "", f"{received:,.0f}" if received else "",
                  f"{mb:,.0f}" if mb else "", f"{lost:.2%}" if lost else ""]
        lines.append("| " + " | ".join(cells) + " |")
    return lines


def peers_text(r):
    args = " ".join(r["peer_args"])
    return f"{r['peers']} peers, each {args}" if r.get("peers", 1) > 1 else args


def section(direction, runs, plots):
    """One direction's plots, a table per backend and what each workload sent."""
    out = [f"## {direction.capitalize()}", ""]
    order = ["base", "hard", "throughput", "latency"]

    def rank(png):
        backend, _, case = png.stem.removeprefix(f"{direction}_").rpartition("_")
        return backend, order.index(case) if case in order else len(order)

    pngs = sorted(plots.glob(f"{direction}_*.png"), key=rank) if plots.is_dir() else []
    if pngs:
        out += ["### Plots", ""]
        out += [f"![{png.stem}](plots/{png.name})" for png in pngs]
        out.append("")

    by_backend = defaultdict(list)
    for r in runs:
        by_backend[r["backend"]].append(r)
    for backend, rs in sorted(by_backend.items()):
        out += [f"### {backend}", "", *table(rs, direction), ""]

    # What each workload sent, per backend since tcp batches and udp splits across peers.
    out += ["### Workloads", ""]
    sent = defaultdict(dict)
    for r in runs:
        sent[r["workload"]].setdefault(r["backend"], peers_text(r))
    for workload, by_backend_text in sent.items():
        texts = set(by_backend_text.values())
        if len(texts) == 1:
            out.append(f"- {workload}: `{texts.pop()}`")
        else:
            out.append(f"- {workload}")
            out += [f"    - {b}: `{t}`" for b, t in sorted(by_backend_text.items())]
    out.append("")
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("results")
    parser.add_argument("--env")
    parser.add_argument("-o", "--out", required=True)
    args = parser.parse_args()

    results = json.loads(pathlib.Path(args.results).read_text())
    runs = results["runs"]
    out = ["# Network benchmarks", "",
           f"_Generated by `tools/netbench`. Started {results['started']}, "
           f"{results['reps']} repetitions per variant, medians of the valid ones._", ""]
    if args.env and pathlib.Path(args.env).exists():
        env = json.loads(pathlib.Path(args.env).read_text())
        out += ["## Fingerprint", ""]
        out += [f"- `{k}`: {v}" for k, v in env.items() if not isinstance(v, (dict, list))]
        out.append("")

    plots = pathlib.Path(args.out).parent / "plots"
    by_direction = defaultdict(list)
    for r in runs:
        by_direction[r.get("direction", "receive")].append(r)
    for direction in ("receive", "send"):
        if direction in by_direction:
            out += section(direction, by_direction[direction], plots)

    void = [r for r in runs if r["void"]]
    if void:
        out += ["## Void runs", ""]
        out += [f"- {r.get('direction', 'receive')} {r['workload']} {r['backend']} / {r['net']} "
                f"rep {r['rep']}: {r['void']}"
                for r in void]
        out.append("")
    pathlib.Path(args.out).write_text("\n".join(out))


if __name__ == "__main__":
    main()
