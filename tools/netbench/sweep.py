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
# The housekeeping CPUs, where capacity peers run so the bench CPUs are the receiver's.
PEER_CPUS = "8-11"


def workload(name, mode, sizes="64", count=20_000, rate=None, burst=None, tcp_batch=None,
             udp_peers=1, drops=False, backends=None, peer_cpus=None):
    """A traffic shape. tcp_batch lets one tcp peer send what is due in one call. udp_peers splits
    the load across that many udp peers, since batching only delays datagrams in the sender.
    backends limits it to those it means something for. peer_cpus runs the peers there, outside the
    bench partition, so they are not what runs out first."""
    return {"name": name, "mode": mode, "sizes": sizes, "count": count, "rate": rate,
            "burst": burst, "tcp_batch": tcp_batch, "udp_peers": udp_peers, "drops_allowed": drops,
            "backends": backends or BACKENDS, "peer_cpus": peer_cpus}


def load_step(rate):
    """Offered load at a fixed rate for about a second and a half. Where losses start or p99
    climbs is the receiver's limit."""
    return workload(f"load_{rate // 1000}k_512", "fixed", sizes="512", count=rate * 3 // 2,
                    rate=rate, tcp_batch=32, udp_peers=4, drops=True, peer_cpus=PEER_CPUS)


def peer_args(w, backend):
    """Every peer's args and the peer count for one backend."""
    peers = w["udp_peers"] if backend == "kernel_udp" else 1
    count = w["count"] // peers
    args = ["--mode", w["mode"], "--sizes", w["sizes"], "--count", str(count),
            "--warmup", str(count // 10)]
    if w["rate"]:
        args += ["--rate", str(w["rate"] // peers)]
    if w["burst"]:
        args += ["--burst", str(w["burst"])]
    if w["tcp_batch"] and backend == "kernel_tcp":
        args += ["--batch", str(w["tcp_batch"])]
    return args, peers


# Each runs for about one to three seconds, each answers one question. The base and hard cases are
# the ones plotted.
SUITES = {
    "transport": [
        # Steady realistic load.
        workload("base_fixed_100k_512", "fixed", sizes="512", count=200_000, rate=100_000),
        # Clumped arrivals, a third of them large and some fragmenting on udp, at a rate that
        # pushes the receiver. Losses are its result.
        workload("hard_poisson_200k_mixed", "poisson", sizes="64:60,1460:30,4000:10",
                 count=400_000, rate=200_000, tcp_batch=16, udp_peers=3, drops=True),
        # Pure latency, one message in flight.
        workload("ack_64", "ack"),
        workload("ack_1460", "ack", sizes="1460"),
        # Realistic bunched arrivals.
        workload("poisson_100k_64", "poisson", count=100_000, rate=100_000),
        # Whether a burst is handled together.
        workload("burst_64x1k_64", "burst", count=128_000, rate=1_000, burst=64, tcp_batch=64,
                 udp_peers=3, drops=True),
        # The throughput limit.
        workload("flood_1460", "flood", sizes="1460", count=600_000, tcp_batch=32, udp_peers=3,
                 drops=True),
    ],
    # What the receiver can take, as load steps and bulk bytes.
    "capacity": [
        *(load_step(rate) for rate in (200_000, 400_000, 800_000, 1_200_000, 1_600_000)),
        # Larger than a udp slot, so tcp only.
        workload("bulk_16k", "flood", sizes="16384", count=200_000, tcp_batch=8,
                 backends=["kernel_tcp"], peer_cpus=PEER_CPUS),
        workload("bulk_64k", "flood", sizes="65000", count=50_000, tcp_batch=8,
                 backends=["kernel_tcp"], peer_cpus=PEER_CPUS),
    ],
}

# Counts that void a run. A flood may lose datagrams, so gaps and drops are its results there.
ALWAYS_VOID = ["late", "bad_framing", "bad_size", "bad_sender", "truncated"]
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


def merge_peers(reports):
    """Several peers' reports as one, counts summed, the longest elapsed, the worst lateness."""
    if not reports or None in reports:
        return None
    merged = dict(reports[0])
    for key in ("count", "warmup", "behind", "missed_acks", "send_errors", "sends"):
        merged[key] = sum(r.get(key, 0) for r in reports)
    for key in ("elapsed_ns", "worst_late_ns"):
        merged[key] = max(r[key] for r in reports)
    merged["peers"] = len(reports)
    return merged


def read_json(path):
    return json.loads(path.read_text()) if path.exists() else None


def run_one(w, backend, args, peers):
    with tempfile.TemporaryDirectory() as out:
        cpus = ["-c", w["peer_cpus"]] if w["peer_cpus"] else []
        cmd = [str(RUN_PAIR), backend, "-s", STAMPS, "-n", str(peers), *cpus, "-o", out, "--",
               *args]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=RUN_TIMEOUT_S)
        except subprocess.TimeoutExpired:
            return None, None, "timeout"
        peer = merge_peers([read_json(pathlib.Path(out, f"peer-{k}.json")) for k in range(peers)])
        receiver = read_json(pathlib.Path(out, "receiver.json"))
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
                    if backend not in w["backends"]:
                        continue
                    run_args, peers = peer_args(w, backend)
                    net_state = set_net(net)
                    if net_state is None:
                        peer, receiver, reason = None, None, "receive path not set"
                    else:
                        peer, receiver, reason = run_one(w, backend, run_args, peers)
                    runs.append({"direction": "receive", "suite": suite, "workload": w["name"],
                                 "backend": backend,
                                 "net": net, "net_state": net_state, "stamps": STAMPS, "rep": rep,
                                 "peer_args": run_args, "peers": peers, "offered": w["rate"],
                                 "peer_cpus": w["peer_cpus"], "void": reason, "peer": peer,
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
