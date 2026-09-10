#!/usr/bin/env python3
"""Honest-sim Python backtest benchmark: throughput vs strategy/risk cost.

The engine calls on_event for every market event and check for every intent,
crossing the pybind boundary and running Python each time. This holds the
dataset fixed and measures that per-event Python cost across complexity tiers,
so the slowdown from a heavier strategy or risk callable is visible directly.

The strategy and risk are plain Python functions, the dataset and cost table
are dummy CSVs generated into a temp dir, and the matcher is the cost-aware
one, so this is the real honest-sim path.

Prereq, build the module with the cost-aware matcher and Python bindings:
  cmake --preset release -DQP_BUILD_PYTHON=ON -DQP_BACKTEST_MATCHER=cost_aware
  cmake --build --preset release --target qp_python_backtest
Run:
  python benchmarks/apps/backtest/python/bench_python_backtest.py
"""
from __future__ import annotations

import datetime as dt
import importlib
import math
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve()
while REPO != REPO.parent and not (REPO / "CMakeLists.txt").exists():
    REPO = REPO.parent


def import_module():
    try:
        return importlib.import_module("qp_python_backtest")
    except ImportError:
        pass
    for so in REPO.glob("**/qp_python_backtest*.so"):
        sys.path.insert(0, str(so.parent))
        try:
            return importlib.import_module("qp_python_backtest")
        except ImportError:
            sys.path.pop(0)
    raise SystemExit(
        "qp_python_backtest not found. Build it first:\n"
        "  cmake --preset release -DQP_BUILD_PYTHON=ON -DQP_BACKTEST_MATCHER=cost_aware\n"
        "  cmake --build --preset release --target qp_python_backtest"
    )


qpb = import_module()

SYMBOLS = ["BTCUSDT", "ETHUSDT"]
DAYS = [dt.date(2024, 1, 1), dt.date(2024, 1, 2)]
FIRST, LAST = DAYS[0].isoformat(), DAYS[-1].isoformat()
BARS_PER_DAY = 1440
FUNDING_HOURS = (0, 8, 16)


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def day_ms(day: dt.date) -> int:
    return int(dt.datetime(day.year, day.month, day.day, tzinfo=dt.timezone.utc).timestamp() * 1000)


def gen_dataset(root: Path) -> int:
    """Write dummy futures+spot klines and futures funding. Returns event count."""
    events = 0
    for sym in SYMBOLS:
        for day in DAYS:
            base = day_ms(day)
            klines = []
            for i in range(BARS_PER_DAY):
                ts = base + i * 60_000
                px = 100.0 + (i % 50) * 0.1
                klines.append(f"{sym},K,{ts},{px},{px + 1},{px - 1},{px},1.0,{ts + 59_999}")
            body = "\n".join(klines) + "\n"
            d = day.isoformat()
            write(root / sym / "futures" / "klines" / f"{sym}-1m-{d}.csv", body)
            write(root / sym / "spot" / "klines" / f"{sym}-1m-{d}.csv", body)
            events += 2 * BARS_PER_DAY
        funding = []
        for day in DAYS:
            base = day_ms(day)
            for h in FUNDING_HOURS:
                funding.append(f"{sym},F,{base + h * 3_600_000},8,0.0001")
        write(root / sym / "futures" / "funding" / f"{sym}-fundingRate-2024-01.csv",
              "\n".join(funding) + "\n")
        events += len(DAYS) * len(FUNDING_HOURS)
    return events


def gen_cost_table(path: Path) -> None:
    """Futures venue 0 (4 bps), spot venue 1 (10 bps), one week row each."""
    hdr = "symbol,venue,week_start,half_spread_bps,impact_bps_per_unit,taker_fee_bps"
    rows = [hdr]
    for sym in SYMBOLS:
        rows.append(f"{sym},0,2024-01-01,1.0,0.01,4.0")
        rows.append(f"{sym},1,2024-01-01,1.5,0.01,10.0")
    write(path, "\n".join(rows) + "\n")


Intent, Order, Side = qpb.Intent, qpb.Order, qpb.Side
RiskDecision, RiskOutcome, EventKind = qpb.RiskDecision, qpb.RiskOutcome, qpb.EventKind


def strat_noop(ev):
    return None


def make_strat_lookup(table):
    """Simple realistic strat: dict lookup per event, emit on funding."""
    def on_event(ev):
        if ev.kind == EventKind.Funding:
            if table.get(ev.symbol, 0.0) >= 0.0:
                return [Intent(ev.symbol, ev.venue, 1.0)]
        return None
    return on_event


def make_strat_heavy(work, table):
    """Complex strat: `work` units of Python math per event, then the lookup."""
    def on_event(ev):
        s = 0.0
        for i in range(work):
            s += math.sin(i) * math.cos(i)
        if ev.kind == EventKind.Funding:
            return [Intent(ev.symbol, ev.venue, 1.0 if s >= -1e9 else -1.0)]
        return None
    return on_event


_oid = [0]


def make_check(work=0):
    def check(intent):
        s = 0.0
        for i in range(work):
            s += math.sin(i)
        _oid[0] += 1
        return RiskDecision(RiskOutcome.Approved,
                            Order(_oid[0], intent.symbol, Side.Buy, intent.venue, 1.0))
    return check


def run_once(data_dir, cost_table, on_event, check):
    ds = qpb.Dataset(str(data_dir), SYMBOLS, FIRST, LAST, str(cost_table))
    bt = qpb.PythonBacktest(ds)
    bt.set_on_event(on_event)
    if check is not None:
        bt.set_check(check)
    t0 = time.perf_counter()
    bt.run()
    return time.perf_counter() - t0


def bench(name, data_dir, cost_table, on_event, check, reps=5):
    run_once(data_dir, cost_table, on_event, check)  # warmup
    best = min(run_once(data_dir, cost_table, on_event, check) for _ in range(reps))
    return name, best


def main():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        data_dir = root / "data"
        cost_table = root / "cost_table.csv"
        n_events = gen_dataset(data_dir)
        gen_cost_table(cost_table)

        table = {}  # empty stand-in prediction table, get() returns the default
        tiers = [
            ("noop (boundary only)", strat_noop, None),
            ("lookup strat, approve risk", make_strat_lookup(table), make_check(0)),
            ("heavy strat w=50", make_strat_heavy(50, table), make_check(0)),
            ("heavy strat w=200", make_strat_heavy(200, table), make_check(0)),
            ("heavy strat+risk w=200", make_strat_heavy(200, table), make_check(200)),
        ]

        print(f"events/run: {n_events}   symbols: {len(SYMBOLS)}   days: {len(DAYS)}\n")
        print(f"{'tier':<30}{'ms/run':>10}{'events/s':>14}{'vs noop':>10}")
        base = None
        for name, on_event, check in tiers:
            _, secs = bench(name, data_dir, cost_table, on_event, check)
            base = base or secs
            print(f"{name:<30}{secs * 1e3:>10.2f}{n_events / secs:>14,.0f}{secs / base:>9.2f}x")


if __name__ == "__main__":
    main()
