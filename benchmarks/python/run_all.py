#!/usr/bin/env python3
"""Single entry point for the python benches.

Run:
  python benchmarks/python/run_all.py                 every bench, default window
  python benchmarks/python/run_all.py --years 1       a shorter window
  python benchmarks/python/run_all.py -k backtest     only names containing backtest
  python benchmarks/python/run_all.py --list          names only, nothing executed
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
WIDTH = 78


def rule(char="="):
    print(char * WIDTH)


def heading(text):
    print()
    rule()
    print(f" {text}")
    rule()


def fmt_secs(seconds):
    if seconds < 60:
        return f"{seconds:.1f}s"
    return f"{int(seconds // 60)}m{seconds % 60:04.1f}s"


def discover(pattern):
    found = sorted(HERE.glob("bench_*.py"))
    return found, [p for p in found if not pattern or pattern in p.stem]


def run_bench(path, forwarded):
    command = [sys.executable, str(path)] + forwarded
    started = time.perf_counter()
    proc = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1
    )
    for line in proc.stdout:
        print(f"  | {line.rstrip()}")
    proc.wait()
    return proc.returncode == 0, time.perf_counter() - started


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-k", "--filter", dest="pattern",
                        help="substring of a bench file name")
    parser.add_argument("--list", action="store_true", help="print names without running")
    args, forwarded = parser.parse_known_args()

    found, selected = discover(args.pattern)
    if not found:
        print(f"no bench_*.py in {HERE}", file=sys.stderr)
        return 2
    if not selected:
        names = ", ".join(p.stem for p in found)
        print(f"no bench matching {args.pattern!r}, have {names}", file=sys.stderr)
        return 2

    if args.list:
        for path in selected:
            print(path.stem)
        return 0

    heading("python benches")
    print(f"  interpreter  {sys.executable}")
    print(f"  benches      {len(selected)}")
    if forwarded:
        print(f"  arguments    {' '.join(forwarded)}  (passed to every bench)")

    started_all = time.perf_counter()
    results = []
    for path in selected:
        heading(path.stem)
        results.append((path.stem, *run_bench(path, forwarded)))

    heading("summary")
    for name, ok, elapsed in results:
        print(f"  {name:<{WIDTH - 24}}{fmt_secs(elapsed):>12}   {'ok' if ok else 'FAILED'}")

    failures = [name for name, ok, _ in results if not ok]
    total = time.perf_counter() - started_all
    print()
    if failures:
        print(f"  FAILED in {fmt_secs(total)}")
        for name in failures:
            print(f"    {name}")
    else:
        print(f"  all benches ran in {fmt_secs(total)}")
    rule()
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
