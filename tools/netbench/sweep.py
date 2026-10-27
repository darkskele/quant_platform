#!/usr/bin/env python3
"""Every rig variant over every workload, into one results file.

    python3 tools/netbench/sweep.py -o results.json [--rig rig.json] [--suite transport] [--reps 3]

A run is the backend under test, the subject, and one or more peers, each its own process. Each
workload runs in both directions, receive with peers sending to the subject and send with the
subject sending to one peer. A run is void when it did not finish or a correctness count is
nonzero.

The rig file says where and how processes run, so nothing here knows the machine:

    {
      "addresses": {"subject": "ip", "peer": "ip"},
      "launch": {"subject": [cmd...], "peer": [cmd...], "isolated_peer": [cmd...]},
      "variants": {"name": {"setup": [cmd...]}, ...}
    }

A launch command goes in front of each process of that role. isolated_peer runs the peers of
workloads that need them off the subject's CPUs, peer when not given. A variant's setup runs
before each of its runs, and the first variant is set again at the end. Variants interleave
within each workload, rotated per repetition, so drift spreads across them. Every key is
optional. Without a rig file everything runs here on 127.0.0.1 as one variant, local.
"""

import argparse
import datetime
import json
import os
import pathlib
import subprocess
import sys
import tempfile

RUN_TIMEOUT_S = 120
PORT = 9100
ACK_PORT = 9101

BACKENDS = ["kernel_udp", "kernel_tcp"]
DIRECTIONS = ["receive", "send"]
STAMPS = "kernel"


def workload(name, mode, sizes="64", count=20_000, rate=None, burst=None, tcp_batch=None,
             udp_peers=1, drops=False, backends=None, isolate=False):
    """A traffic shape. tcp_batch lets one tcp peer send what is due in one call. udp_peers splits
    the load across that many udp peers, since batching only delays datagrams in the sender.
    backends limits it to those it means something for. isolate keeps the peers off the subject's
    CPUs, so the subject is what runs out first."""
    return {"name": name, "mode": mode, "sizes": sizes, "count": count, "rate": rate,
            "burst": burst, "tcp_batch": tcp_batch, "udp_peers": udp_peers, "drops_allowed": drops,
            "backends": backends or BACKENDS, "isolate": isolate}


def load_step(rate):
    """Offered load at a fixed rate for about a second and a half. Where losses start or p99
    climbs is the subject's limit."""
    return workload(f"load_{rate // 1000}k_512", "fixed", sizes="512", count=rate * 3 // 2,
                    rate=rate, tcp_batch=32, udp_peers=4, drops=True, isolate=True)


def peer_args(w, backend, direction):
    """Every sender's traffic args and the peer count for one backend and direction. Sending, the
    backend is the one sender."""
    peers = w["udp_peers"] if backend == "kernel_udp" and direction == "receive" else 1
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
    # What the subject can take, as load steps and bulk bytes.
    "capacity": [
        *(load_step(rate) for rate in (200_000, 400_000, 800_000, 1_200_000, 1_600_000)),
        # Larger than a udp slot, so tcp only.
        workload("bulk_16k", "flood", sizes="16384", count=200_000, tcp_batch=8,
                 backends=["kernel_tcp"], isolate=True),
        workload("bulk_64k", "flood", sizes="65000", count=50_000, tcp_batch=8,
                 backends=["kernel_tcp"], isolate=True),
    ],
}

# Counts that void a run. A flood may lose datagrams, so gaps and drops are its results there.
ALWAYS_VOID = ["late", "bad_framing", "bad_size", "bad_sender", "truncated"]
LOSS = ["gaps", "dropped"]


def void_reason(w, sender, receiver):
    if receiver is None or sender is None:
        return "no report"
    if not receiver.get("completed"):
        return "did not finish"
    counters = receiver["counters"]
    checks = ALWAYS_VOID + ([] if w["drops_allowed"] else LOSS)
    bad = [f"{k}={counters[k]}" for k in checks if counters.get(k)]
    if sender.get("missed_acks"):
        bad.append(f"missed_acks={sender['missed_acks']}")
    if sender.get("send_errors") and not w["drops_allowed"]:
        bad.append(f"send_errors={sender['send_errors']}")
    return ", ".join(bad) or None


class Rig:
    """Where processes run, read from the rig file, or everything here when there is none."""

    def __init__(self, path):
        spec = json.loads(pathlib.Path(path).read_text()) if path else {}
        addresses = spec.get("addresses", {})
        self.subject_ip = addresses.get("subject", "127.0.0.1")
        self.peer_ip = addresses.get("peer", "127.0.0.1")
        launch = spec.get("launch", {})
        self.subject = launch.get("subject", [])
        self.peer = launch.get("peer", [])
        self.isolated_peer = launch.get("isolated_peer", self.peer)
        self.variants = spec.get("variants", {"local": {}})

    def setup(self, variant):
        """False when the variant's setup failed."""
        cmd = self.variants[variant].get("setup")
        if not cmd:
            return True
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"sweep: setup of {variant} failed: {proc.stderr.strip()}", file=sys.stderr)
        return proc.returncode == 0


def commands(rig, w, backend, peers, args, direction, out):
    """The peers' commands, then the subject's. A TCP subject receiving connects to the peer, a
    UDP one is sent to. Sending, the subject connects to or targets the peer."""
    bin_dir = pathlib.Path(os.environ.get("QP_BUILD_DIR", "build/release"))
    peer_bin, subject_bin = str(bin_dir / "netbench_peer"), str(bin_dir / f"netbench_{backend}")
    launch_peer = rig.isolated_peer if w["isolate"] else rig.peer
    proto = "tcp" if backend == "kernel_tcp" else "udp"
    peer_at = f"{rig.peer_ip}:{PORT}"
    if direction == "send":
        ack = f"{rig.subject_ip}:{ACK_PORT}"
        peer = [*launch_peer, peer_bin, "--role", "receive", "--proto", proto, "--target", peer_at,
                "--ack", ack, "--out", str(out / "receiver.json")]
        subject = [*rig.subject, subject_bin, "send", "--target", peer_at, "--ack", ack,
                   "--out", str(out / "sender.json"), *args]
        return [peer], subject
    target = peer_at if proto == "tcp" else f"{rig.subject_ip}:{PORT}"
    # After the shared args, so each peer's own id, seed and ack port win.
    senders = [[*launch_peer, peer_bin, "--proto", proto, "--target", target,
                "--out", str(out / f"peer-{k}.json"), *args, "--sender", str(k),
                "--seed", str(k + 1), "--ack", f"{rig.peer_ip}:{ACK_PORT + k}"]
               for k in range(peers)]
    subject = [*rig.subject, subject_bin, "--target", target, "--ack",
               f"{rig.peer_ip}:{ACK_PORT}", "--peers", str(peers), "--stamps", STAMPS,
               "--out", str(out / "receiver.json")]
    return senders, subject


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


def run_one(rig, w, backend, args, peers, direction):
    """The sender's and receiver's reports and why the run is void. Receiving, the peers are the
    sender, merged. Sending, the subject is."""
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp)
        peer_cmds, subject_cmd = commands(rig, w, backend, peers, args, direction, out)
        log = out / "log.txt"
        timed_out = False
        with log.open("w") as err:
            procs = [subprocess.Popen(c, stdout=subprocess.DEVNULL, stderr=err) for c in peer_cmds]
            procs.append(subprocess.Popen(subject_cmd, stdout=subprocess.DEVNULL, stderr=err))
            try:
                for p in procs:
                    p.wait(timeout=RUN_TIMEOUT_S)
            except subprocess.TimeoutExpired:
                timed_out = True
                # Terminate first, since a launch command such as sudo passes it on and a kill
                # would leave the program running.
                for p in procs:
                    p.terminate()
                for p in procs:
                    try:
                        p.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        p.kill()
                        p.wait()
        if timed_out:
            return None, None, "timeout"
        if direction == "send":
            sender = read_json(out / "sender.json")
        else:
            sender = merge_peers([read_json(out / f"peer-{k}.json") for k in range(peers)])
        receiver = read_json(out / "receiver.json")
        reason = void_reason(w, sender, receiver)
        lines = log.read_text().strip().splitlines()
        if reason == "no report" and lines:
            # The last lines say what failed.
            reason += ": " + " | ".join(lines[-4:])
        return sender, receiver, reason


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--out", required=True)
    parser.add_argument("--rig", help="where and how processes run, everything here when absent")
    parser.add_argument("--suite", choices=[*SUITES, "all"], default="all")
    parser.add_argument("--reps", type=int, default=3)
    parser.add_argument("--only", help="run only workloads whose name contains this")
    parser.add_argument("--direction", choices=[*DIRECTIONS, "both"], default="both")
    args = parser.parse_args()

    rig = Rig(args.rig)
    suites = SUITES if args.suite == "all" else {args.suite: SUITES[args.suite]}
    directions = DIRECTIONS if args.direction == "both" else [args.direction]
    variants = [(b, v) for b in BACKENDS for v in rig.variants]
    started = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
    runs = []
    for rep in range(args.reps):
        for direction in directions:
            for suite, workloads in suites.items():
                for w in workloads:
                    if args.only and args.only not in w["name"]:
                        continue
                    k = rep % len(variants)
                    for backend, variant in variants[k:] + variants[:k]:
                        if backend not in w["backends"]:
                            continue
                        run_args, peers = peer_args(w, backend, direction)
                        if rig.setup(variant):
                            sender, receiver, reason = run_one(rig, w, backend, run_args, peers,
                                                               direction)
                        else:
                            sender, receiver, reason = None, None, "variant setup failed"
                        runs.append({"direction": direction, "suite": suite,
                                     "workload": w["name"], "backend": backend,
                                     "variant": variant, "stamps": STAMPS, "rep": rep,
                                     "peer_args": run_args, "peers": peers, "offered": w["rate"],
                                     "isolate": w["isolate"], "void": reason,
                                     "sender": sender, "receiver": receiver})
                        state = f"void, {reason}" if reason else "ok"
                        print(f"sweep: rep {rep} {direction} {w['name']} {backend} {variant}: "
                              f"{state}", file=sys.stderr, flush=True)
    rig.setup(next(iter(rig.variants)))

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({"started": started, "reps": args.reps,
                               "variants": list(rig.variants), "runs": runs}) + "\n")
    void = sum(1 for r in runs if r["void"])
    print(f"sweep: {len(runs)} runs, {void} void, results in {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
