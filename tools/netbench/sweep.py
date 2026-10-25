#!/usr/bin/env python3
"""Every rig variant over every workload, into one results file. Runs on the box in bench mode.

    python3 tools/netbench/sweep.py -o benchmarks/network/results/results.json [--suite transport] [--reps 3]

A variant is a backend and a receive path. Variants interleave within each workload, rotated per
repetition, so drift spreads across them. Every run takes kernel stamps. A run is void when it
did not finish or a correctness count is nonzero.
"""

import argparse
import datetime
import json
import pathlib
import subprocess
import sys
import tempfile

RUN_PAIR = pathlib.Path(__file__).with_name("run_pair.sh")
RUN_TIMEOUT_S = 120

BACKENDS = ["kernel_udp", "kernel_tcp"]
# How qp-rx processes what arrives, set through qp-netns-config. napi takes it off the sender's
# core, as a real receive path would be.
NETS = ["napi"]
STAMPS = "kernel"


def workload(name, mode, sizes="64", count=20_000, rate=None, burst=None, drops=False):
    args = ["--mode", mode, "--sizes", sizes, "--count", str(count), "--warmup", str(count // 10)]
    if rate:
        args += ["--rate", str(rate)]
    if burst:
        args += ["--burst", str(burst)]
    return {"name": name, "peer_args": args, "drops_allowed": drops}


# Each runs for about one to three seconds. The base and hard cases are the ones plotted.
SUITES = {
    "transport": [
        workload("base_fixed_100k_512", "fixed", sizes="512", count=200_000, rate=100_000),
        # Clumped arrivals, a third of them large and some fragmenting on udp, at a rate that
        # pushes the receiver. Losses are its result.
        workload("hard_poisson_200k_mixed", "poisson", sizes="64:60,1460:30,4000:10",
                 count=400_000, rate=200_000, drops=True),
        workload("ack_64", "ack"),
        workload("ack_512", "ack", sizes="512"),
        workload("ack_1460", "ack", sizes="1460"),
        # Past one packet, so udp fragments and tcp segments.
        workload("ack_4000", "ack", sizes="4000"),
        workload("fixed_1k_64", "fixed", count=3_000, rate=1_000),
        workload("fixed_10k_64", "fixed", count=30_000, rate=10_000),
        workload("fixed_100k_64", "fixed", count=100_000, rate=100_000),
        workload("poisson_10k_64", "poisson", count=30_000, rate=10_000),
        workload("poisson_100k_64", "poisson", count=100_000, rate=100_000),
        workload("poisson_10k_mixed", "poisson", sizes="64:90,1460:10", count=30_000, rate=10_000),
        workload("burst_8x1k_64", "burst", count=24_000, rate=1_000, burst=8),
        workload("burst_64x1k_64", "burst", count=128_000, rate=1_000, burst=64),
        workload("flood_64", "flood", count=200_000, drops=True),
        workload("flood_1460", "flood", sizes="1460", count=100_000, drops=True),
    ],
}

# Counts that void a run. A flood may lose datagrams, so gaps and drops are its results there.
ALWAYS_VOID = ["late", "bad_framing", "bad_size", "truncated"]
LOSS = ["gaps", "dropped"]


def void_reason(w, peer, receiver):
    if receiver is None or peer is None:
        return "no report"
    if not receiver.get("completed"):
        return "did not finish"
    counters = receiver["counters"]
    checks = ALWAYS_VOID + ([] if w["drops_allowed"] else LOSS)
    bad = [f"{k}={counters[k]}" for k in checks if counters.get(k)]
    if peer.get("missed_acks"):
        bad.append(f"missed_acks={peer['missed_acks']}")
    if peer.get("send_errors") and not w["drops_allowed"]:
        bad.append(f"send_errors={peer['send_errors']}")
    return ", ".join(bad) or None


def set_net(net):
    """The receive path state qp-netns-config left, or None when it failed."""
    proc = subprocess.run(["sudo", "-n", "qp-netns-config", net], capture_output=True, text=True)
    if proc.returncode != 0:
        print(f"sweep: qp-netns-config {net} failed: {proc.stderr.strip()}", file=sys.stderr)
        return None
    return json.loads(proc.stdout.strip().splitlines()[-1])


def run_one(w, backend):
    with tempfile.TemporaryDirectory() as out:
        cmd = [str(RUN_PAIR), backend, "-s", STAMPS, "-o", out, "--", *w["peer_args"]]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=RUN_TIMEOUT_S)
        except subprocess.TimeoutExpired:
            return None, None, "timeout"
        reports = []
        for name in ("peer.json", "receiver.json"):
            path = pathlib.Path(out, name)
            reports.append(json.loads(path.read_text()) if path.exists() else None)
        peer, receiver = reports
        reason = void_reason(w, peer, receiver)
        if reason == "no report" and proc.stderr:
            # The last lines before run_pair's own summary say what failed.
            reason += ": " + " | ".join(proc.stderr.strip().splitlines()[-4:])
        return peer, receiver, reason


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--out", required=True)
    parser.add_argument("--suite", choices=[*SUITES, "all"], default="all")
    parser.add_argument("--reps", type=int, default=3)
    parser.add_argument("--only", help="run only workloads whose name contains this")
    args = parser.parse_args()

    suites = SUITES if args.suite == "all" else {args.suite: SUITES[args.suite]}
    variants = [(b, n) for b in BACKENDS for n in NETS]
    started = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
    runs = []
    for rep in range(args.reps):
        for suite, workloads in suites.items():
            for w in workloads:
                if args.only and args.only not in w["name"]:
                    continue
                k = rep % len(variants)
                for backend, net in variants[k:] + variants[:k]:
                    net_state = set_net(net)
                    if net_state is None:
                        peer, receiver, reason = None, None, "receive path not set"
                    else:
                        peer, receiver, reason = run_one(w, backend)
                    runs.append({"suite": suite, "workload": w["name"], "backend": backend,
                                 "net": net, "net_state": net_state, "stamps": STAMPS, "rep": rep,
                                 "peer_args": w["peer_args"], "void": reason, "peer": peer,
                                 "receiver": receiver})
                    state = f"void, {reason}" if reason else "ok"
                    print(f"sweep: rep {rep} {w['name']} {backend} {net}: {state}",
                          file=sys.stderr, flush=True)
    set_net(NETS[0])

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"started": started, "reps": args.reps, "runs": runs}) + "\n")
    void = sum(1 for r in runs if r["void"])
    print(f"sweep: {len(runs)} runs, {void} void, results in {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
