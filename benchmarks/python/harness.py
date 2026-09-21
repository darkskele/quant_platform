#!/usr/bin/env python3
"""Profiles a backtest scale Binance historical run through the Python bindings.

Build the module first:
  cmake --preset release -DQP_BUILD_PYTHON=ON -DPYTHON_EXECUTABLE=$HOME/miniconda3/envs/qp-research/bin/python
  cmake --build build/release --target qp_python_backtest -j
Run:
  python benchmarks/python/bench_python_backtest.py
  python benchmarks/python/bench_python_backtest.py --years 1 --tiers
  python benchmarks/python/bench_python_backtest.py --years 1 --sweep 4,8,16,32,64
"""
from __future__ import annotations

import datetime as dt
import importlib
import resource
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve()
while REPO != REPO.parent and not (REPO / "CMakeLists.txt").exists():
    REPO = REPO.parent

perf = time.perf_counter_ns


def import_module():
    try:
        return importlib.import_module("qp_python_backtest")
    except ImportError:
        pass

    found = sorted(REPO.glob("**/qp_python_backtest*.so"))
    for so in found:
        sys.path.insert(0, str(so.parent))
        try:
            return importlib.import_module("qp_python_backtest")
        except ImportError:
            sys.path.pop(0)

    # A build for another interpreter is the common case with two pythons on
    # the box, and it looks identical to no build at all without this.
    if found:
        built = "\n".join(f"  {so.name}" for so in found)
        raise SystemExit(
            f"qp_python_backtest was built, but not for {sys.executable}\n"
            f"(python {sys.version_info.major}.{sys.version_info.minor})\n"
            f"{built}\n"
            "Rebuild with -DPYTHON_EXECUTABLE pointed at this interpreter, or run"
            " the one it was built for."
        )
    raise SystemExit(
        "qp_python_backtest not found. Build it first:\n"
        "  cmake --preset release -DQP_BUILD_PYTHON=ON\n"
        "  cmake --build build/release --target qp_python_backtest -j"
    )


import_started = perf()
qpb = import_module()
IMPORT_NS = perf() - import_started

EventKind = qpb.EventKind
Intent, Order, Side = qpb.Intent, qpb.Order, qpb.Side
RiskDecision, RiskOutcome = qpb.RiskDecision, qpb.RiskOutcome

# A fixed window rather than one relative to today, so two runs months apart
# are comparable. Both symbols list well before it on both markets.
WINDOW_START = dt.date(2020, 1, 1)
SYMBOLS = ["BTCUSDT", "ETHUSDT"]
MARKETS = [qpb.BinanceMarket.UsdM, qpb.BinanceMarket.Spot]

# Funding is a futures dataset. The source skips it for spot rather than
# planning files that are not there.
STREAMS = [
    qpb.StreamSpec(qpb.EndpointKind.Klines, "1h"),
    qpb.StreamSpec(qpb.EndpointKind.FundingRate),
]

NS_PER_DAY = 86_400_000_000_000


def to_ns(day):
    return int(dt.datetime(day.year, day.month, day.day, tzinfo=dt.timezone.utc).timestamp() * 1e9)


# ------------------------------------------------------------------ statistics


def percentile(ordered, fraction):
    if not ordered:
        return 0
    index = int(fraction * (len(ordered) - 1))
    return ordered[index]


def summarize(samples):
    """Ordered copy plus the headline percentiles, in nanoseconds."""
    ordered = sorted(samples)
    total = sum(ordered)
    return {
        "n": len(ordered),
        "total_ns": total,
        "mean": total / len(ordered) if ordered else 0,
        "min": ordered[0] if ordered else 0,
        "p50": percentile(ordered, 0.50),
        "p90": percentile(ordered, 0.90),
        "p99": percentile(ordered, 0.99),
        "p999": percentile(ordered, 0.999),
        "max": ordered[-1] if ordered else 0,
        "ordered": ordered,
    }


def fmt_ns(value):
    value = float(value)
    if value < 1_000:
        return f"{value:,.0f}ns"
    if value < 1_000_000:
        return f"{value / 1e3:,.1f}us"
    if value < 1_000_000_000:
        return f"{value / 1e6:,.1f}ms"
    return f"{value / 1e9:,.2f}s"


def histogram(ordered, buckets=14, width=44):
    """Log spaced buckets, since the tail is the interesting part."""
    if not ordered:
        return ["  (no samples)"]
    low = max(ordered[0], 1)
    high = max(ordered[-1], low + 1)
    ratio = (high / low) ** (1.0 / buckets)

    edges = [low * (ratio ** i) for i in range(buckets + 1)]
    counts = [0] * buckets
    cursor = 0
    for value in ordered:
        while cursor < buckets - 1 and value > edges[cursor + 1]:
            cursor += 1
        counts[cursor] += 1

    peak = max(counts) or 1
    lines = []
    for i, count in enumerate(counts):
        if count == 0:
            continue
        bar = "#" * max(1, int(width * count / peak))
        share = 100.0 * count / len(ordered)
        lines.append(
            f"  {fmt_ns(edges[i]):>9} .. {fmt_ns(edges[i + 1]):>9}  "
            f"{count:>9,} {share:>5.1f}%  {bar}"
        )
    return lines


def print_distribution(title, stats):
    print(f"\n{title}   n={stats['n']:,}  total={fmt_ns(stats['total_ns'])}")
    print(
        f"  min {fmt_ns(stats['min'])}   p50 {fmt_ns(stats['p50'])}   "
        f"p90 {fmt_ns(stats['p90'])}   p99 {fmt_ns(stats['p99'])}   "
        f"p99.9 {fmt_ns(stats['p999'])}   max {fmt_ns(stats['max'])}   "
        f"mean {fmt_ns(stats['mean'])}"
    )
    for line in histogram(stats["ordered"]):
        print(line)


# ------------------------------------------------------------------ the run


def clock_overhead_ns(iterations=200_000):
    """What a perf_counter_ns pair costs, since the callback timings include one."""
    started = perf()
    for _ in range(iterations):
        perf()
    return (perf() - started) / iterations


class Profile:
    def __init__(self):
        self.callback_ns = []
        self.gap_ns = []
        self.risk_ns = []
        self.orders = 0
        self.events = 0


def make_callbacks(profile, funding_slots, realistic):
    """A carry shaped strategy, or a bare return for the baseline."""
    callback_ns = profile.callback_ns
    gap_ns = profile.gap_ns
    risk_ns = profile.risk_ns
    last_end = [0]
    last_close = {}

    def on_event(event):
        start = perf()
        if last_end[0]:
            gap_ns.append(start - last_end[0])

        out = None
        if realistic:
            base = event.base
            kind = base.kind
            key = (base.market, base.symbol)
            if kind == EventKind.Kline:
                last_close[key] = event.payload.close
            elif kind == EventKind.Funding:
                rate = event.payload.funding_rate
                close = last_close.get(key)
                if close is not None and rate > 0.0 and key in funding_slots:
                    out = [Intent(base.exchange, base.market, base.symbol, 1.0)]

        end = perf()
        callback_ns.append(end - start)
        last_end[0] = end
        return out

    def check(intent):
        start = perf()
        profile.orders += 1
        decision = RiskDecision(
            RiskOutcome.Approved,
            Order(
                profile.orders,
                intent.exchange,
                intent.market,
                intent.symbol,
                Side.Buy,
                intent.target_position,
            ),
        )
        risk_ns.append(perf() - start)
        return decision

    return on_event, check


def build(years, workers):
    phases = {}

    started = perf()
    builder = qpb.SubscriptionBuilder()
    for market in MARKETS:
        for symbol in SYMBOLS:
            builder.add(qpb.ExchangeId.Binance, int(market), symbol)
    subscription = builder.build()
    phases["subscription"] = perf() - started

    started = perf()
    pool = qpb.FetchPoolConfig()
    pool.workers = workers
    end_day = dt.date(WINDOW_START.year + years, WINDOW_START.month, WINDOW_START.day)
    config = qpb.BinanceHistoricalConfig(
        STREAMS, qpb.Cadence.Monthly, to_ns(WINDOW_START), to_ns(end_day), pool
    )
    phases["config"] = perf() - started

    started = perf()
    backtest = qpb.PythonBacktest(subscription, config)
    phases["construct"] = perf() - started

    usd_m = int(qpb.BinanceMarket.UsdM)
    funding_slots = set()
    for symbol in SYMBOLS:
        instrument = subscription.resolve(qpb.ExchangeId.Binance, usd_m, symbol)
        if instrument is not None:
            funding_slots.add((usd_m, instrument.symbol))

    return backtest, phases, funding_slots, end_day


def run(years, workers, tier):
    """tier is one of the entries in TIERS, which fixes what python is asked to do."""
    backtest, phases, funding_slots, end_day = build(years, workers)
    profile = Profile()

    started = perf()
    backtest.plan()
    phases["plan"] = perf() - started

    on_event, check = make_callbacks(profile, funding_slots, tier == "strategy")
    if tier == "no python":
        # A kind this run never produces, so the mask short circuits before the
        # GIL is taken and nothing crosses the boundary at all.
        backtest.set_event_kinds([EventKind.BookDepth])
    else:
        backtest.set_on_event(on_event)
    if tier == "strategy":
        backtest.set_check(check)

    started = perf()
    backtest.run()
    phases["run"] = perf() - started

    started = perf()
    results = backtest.results()
    phases["results"] = perf() - started

    profile.events = len(profile.callback_ns)
    return backtest, profile, phases, results, end_day


# ------------------------------------------------------------------ reporting


def report(backtest, profile, phases, results, years, end_day, overhead, workers):
    reports = backtest.reports()
    stats = backtest.fetch_stats()
    parsed = sum(r.stats.rows_parsed for r in reports)
    planned = sum(r.stats.files_planned for r in reports)

    # A row count can only be predicted from an interval. Funding has none, so
    # summing expected across every stream compares bars against nothing and
    # always reads as a surplus.
    predictable = [r for r in reports if r.stats.rows_expected]
    predicted_parsed = sum(r.stats.rows_parsed for r in predictable)
    predicted = sum(r.stats.rows_expected for r in predictable)
    unpredictable = parsed - predicted_parsed

    print("=" * 78)
    print(
        f"window {WINDOW_START} to {end_day} ({years}y)   "
        f"markets {len(MARKETS)}   symbols {len(SYMBOLS)}   streams {len(reports)}   "
        f"workers {workers}"
    )
    print(
        f"files planned {planned:,}   fetched {stats.completed_ok:,}   "
        f"failed {stats.completed_failed:,}   retries {stats.retries:,}"
    )
    if stats.completed_failed or stats.refused or stats.cancellations_dropped:
        print(
            f"  failures     404 {stats.not_found:,}   5xx {stats.server_error:,}"
            f"   transport {stats.transport_error:,}   zip {stats.zip_error:,}"
            f"   cancelled {stats.cancelled:,}"
        )
        print(
            f"  pressure     refused {stats.refused:,}"
            f"   dropped {stats.cancellations_dropped:,}"
            f"   critical {stats.critical}"
        )
        for failure in backtest.fetch_failures()[:5]:
            kind = str(failure.status).split(".")[-1]
            print(f"    {kind:<16}{failure.attempts} tries  {failure.url}")
    print(
        f"pool  workers {stats.workers_alive:,} alive   queued {stats.queued:,}"
        f"   in flight {stats.in_flight:,}   oldest {stats.oldest_in_flight_ms:,}ms"
    )
    shortfall = predicted - predicted_parsed
    print(
        f"rows parsed {parsed:,}   orders {profile.orders:,}   "
        f"final equity {results.final_equity:,.2f}"
    )
    print(
        f"  intervalled  {predicted_parsed:,} of {predicted:,} expected"
        f"   shortfall {shortfall:,}   over {len(predictable)} streams"
    )
    print(
        f"  intervalless {unpredictable:,} parsed"
        f"   over {len(reports) - len(predictable)} streams, nothing to predict from"
    )

    run_s = phases["run"] / 1e9
    fetched_mb = stats.bytes_fetched / 1e6
    inflated_mb = stats.bytes_inflated / 1e6
    print(
        f"transfer {fetched_mb:,.1f} MB -> {inflated_mb:,.1f} MB inflated   "
        f"{fetched_mb / run_s:,.1f} MB/s over the run"
    )

    peak_rss_mb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024
    print(f"peak rss {peak_rss_mb:,.0f} MB   perf_counter_ns overhead {fmt_ns(overhead)}")

    print("\nphases")
    ordered = ["subscription", "config", "construct", "plan", "run", "results"]
    total = sum(phases[name] for name in ordered) + IMPORT_NS
    print(f"  {'module import':<16}{fmt_ns(IMPORT_NS):>12}{100 * IMPORT_NS / total:>8.1f}%")
    for name in ordered:
        value = phases[name]
        print(f"  {name:<16}{fmt_ns(value):>12}{100 * value / total:>8.1f}%")
    print(f"  {'total':<16}{fmt_ns(total):>12}")

    if profile.events:
        print(f"\nthroughput  {profile.events / run_s:,.0f} events/s over the whole run")
        print_distribution("strategy callback (python cost, clock included)",
                           summarize(profile.callback_ns))
        print_distribution("between callbacks (fetch, parse, merge and engine)",
                           summarize(profile.gap_ns))
    if profile.risk_ns:
        print_distribution("risk check (python cost)", summarize(profile.risk_ns))

    print("\nper stream")
    print(f"  {'market':>7} {'symbol':<10}{'kind':<20}{'files':>7}{'rows':>12}{'shortfall':>11}")
    for r in reports:
        gap = f"{r.stats.rows_expected - r.stats.rows_parsed:,}" if r.stats.rows_expected else "-"
        print(
            f"  {r.market:>7} {r.symbol:<10}{str(r.kind).split('.')[-1]:<20}"
            f"{r.stats.files_read:>7}{r.stats.rows_parsed:>12,}{gap:>11}"
        )



TIERS = [
    ("no python", "fetch, unzip, parse, merge and engine, nothing crosses the boundary"),
    ("empty callback", "the same plus one python call per event that does nothing"),
    ("strategy", "the same plus a carry shaped strategy and an approving risk gate"),
]


def rule(char="="):
    print(char * 78)


def heading(text):
    print()
    rule()
    print(f" {text}")
    rule()
