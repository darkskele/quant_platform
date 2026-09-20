#!/usr/bin/env python3
"""Runs the python tests beside this script.

Run:
  python tests/python/run_all.py                every test
  python tests/python/run_all.py timer          only names containing timer
  python tests/python/run_all.py --list         names only, nothing executed
"""
from __future__ import annotations

import argparse
import importlib.util
import sys
import time
import traceback
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


def row(status, name, seconds):
    print(f"  {status:<6}{name:<{WIDTH - 18}}{seconds:>9.1f}s")


def fmt_secs(seconds):
    if seconds < 60:
        return f"{seconds:.1f}s"
    return f"{int(seconds // 60)}m{seconds % 60:04.1f}s"


def load_module(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def discover(pattern):
    """Yields (module_name, function_name, callable) for every matching test."""
    for path in sorted(HERE.glob("test_*.py")):
        module = load_module(path)
        for name in sorted(vars(module)):
            if not name.startswith("test_"):
                continue
            fn = getattr(module, name)
            if not callable(fn):
                continue
            if pattern and pattern not in name and pattern not in path.stem:
                continue
            yield path.stem, name, fn


def format_failure():
    """The traceback without this runner's own frame on top of it, printed to
    stdout so it stays beside its row when the output is piped."""
    kind, value, trace = sys.exc_info()
    formatted = traceback.TracebackException(kind, value, trace)
    if len(formatted.stack) > 1:
        formatted.stack = traceback.StackSummary.from_list(formatted.stack[1:])
    return "".join(formatted.format())


def run_tests(selected):
    results = []
    for module_name, name, fn in selected:
        label = f"{module_name}::{name}"
        started = time.perf_counter()
        try:
            fn()
        except Exception:
            elapsed = time.perf_counter() - started
            row("FAIL", label, elapsed)
            for line in format_failure().splitlines():
                print(f"        {line}")
            results.append((label, elapsed, False))
        else:
            elapsed = time.perf_counter() - started
            row("ok", label, elapsed)
            results.append((label, elapsed, True))
    return results


def summary(results, total):
    heading("summary")

    passed = sum(1 for _, _, ok in results if ok)
    failed = len(results) - passed
    print(f"  {passed:>4} passed{failed:>5} failed{sum(e for _, e, _ in results):>11.1f}s")

    slowest = sorted(results, key=lambda r: -r[1])[:5]
    if slowest:
        print("\n  slowest")
        for label, elapsed, _ in slowest:
            print(f"    {label:<{WIDTH - 20}}{elapsed:>9.1f}s")

    failures = [label for label, _, ok in results if not ok]
    print()
    if failures:
        print(f"  FAILED in {fmt_secs(total)}")
        for label in failures:
            print(f"    {label}")
    else:
        print(f"  all green in {fmt_secs(total)}")
    rule()
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pattern", nargs="?", help="substring of a test or file name")
    parser.add_argument("--list", action="store_true", help="print names without running")
    args = parser.parse_args()

    # The tests import their helpers as plain modules from this directory.
    sys.path.insert(0, str(HERE))

    try:
        selected = list(discover(args.pattern))
    except SystemExit as reason:
        print(reason, file=sys.stderr)
        return 2

    if not selected:
        print(f"no test matching {args.pattern!r}", file=sys.stderr)
        return 2

    if args.list:
        for module_name, name, _ in selected:
            print(f"{module_name}::{name}")
        return 0

    heading(f"python tests  ({len(selected)})")
    print(f"  interpreter  {sys.executable}")
    print()

    started_all = time.perf_counter()
    results = run_tests(selected)
    return summary(results, time.perf_counter() - started_all)


if __name__ == "__main__":
    sys.exit(main())
