"""Panels streamed from the Binance archive through the qp source.

Daily bars keyed on the bar's close time, so a bar is dated the day it was
known. Monthly files skip some days that the daily files carry, so any day
missing inside a symbol's own span is refilled from the daily archive. Rows are
clipped to the window, since the source keeps whole files.

Pulled tables are cached under ~/.cache/qp-research. The archive stays the only
source, so deleting the cache just means the next pull streams again.
"""
from __future__ import annotations

import glob
import re
import sys
import urllib.request
from pathlib import Path

import pandas as pd

REPO = Path(__file__).resolve().parents[2]
CACHE = Path.home() / ".cache" / "qp-research"
LISTING = "https://s3-ap-northeast-1.amazonaws.com/data.binance.vision?delimiter=/&prefix="


def module():
    """The built qp_python_backtest extension."""
    found = sorted(glob.glob(str(REPO / "build/release/**/qp_python_backtest*.so"), recursive=True))
    if not found:
        raise SystemExit("qp_python_backtest not built")
    path = str(Path(found[0]).parent)
    if path not in sys.path:
        sys.path.insert(0, path)
    import qp_python_backtest

    return qp_python_backtest


def usdm_perps() -> list[str]:
    """Every USDT-margined perpetual the archive has ever carried, delisted included."""
    prefix = "data/futures/um/monthly/klines/"
    names, marker = [], ""
    while True:
        xml = urllib.request.urlopen(LISTING + prefix + (f"&marker={marker}" if marker else "")).read().decode()
        names += re.findall(rf"<Prefix>{prefix}([^/<]+)/</Prefix>", xml)
        if "<IsTruncated>true</IsTruncated>" not in xml:
            break
        marker = re.search(r"<NextMarker>([^<]+)</NextMarker>", xml).group(1)
    return sorted(n for n in names if n.endswith("USDT") and "_" not in n)


def non_crypto() -> set[str]:
    """USDT perps whose underlying is not crypto, by Binance's own tags.

    TradFi perpetuals cover equities, ETFs, commodities, FX and pre-IPO names.
    RWA tokens such as tokenised gold are tagged crypto but track the real
    asset, so they are excluded too. Every TradFi perp is live, so the current
    exchange info covers them all.
    """
    path = CACHE / "usdm_exchange_info.json"
    if not path.exists():
        CACHE.mkdir(parents=True, exist_ok=True)
        path.write_bytes(urllib.request.urlopen("https://fapi.binance.com/fapi/v1/exchangeInfo").read())
    import json
    info = json.loads(path.read_text())
    return {s["symbol"] for s in info["symbols"]
            if s.get("contractType") == "TRADIFI_PERPETUAL" or "RWA" in s.get("underlyingSubType", [])}


def _ns(day) -> int:
    t = pd.Timestamp(day)
    t = t.tz_localize("UTC") if t.tzinfo is None else t.tz_convert("UTC")
    return int(t.value)


def _collect(symbols, specs, cadence, start, end, workers=32):
    """Kline and funding rows for symbols between start and end, plus the stream reports."""
    q = module()
    um = int(q.BinanceMarket.UsdM)
    builder = q.SubscriptionBuilder()
    for s in symbols:
        builder.add(q.ExchangeId.Binance, um, s)
    sub = builder.build()
    name = {sub.resolve(q.ExchangeId.Binance, um, s).symbol: s for s in symbols}

    pool = q.FetchPoolConfig()
    pool.workers = workers
    # The source keeps a file when its start lies in [from, to], so snap from
    # back to the month and stop to short of end.
    first = pd.Timestamp(start)
    if cadence == q.Cadence.Monthly:
        first = first.replace(day=1)
    cfg = q.BinanceHistoricalConfig(specs, cadence, _ns(first), _ns(end) - 1, pool)
    bt = q.PythonBacktest(sub, cfg)

    bars, prints = [], []
    kline, funding = q.EventKind.Kline, q.EventKind.Funding

    def on_event(e):
        b = e.base
        if b.kind == kline:
            p = e.payload
            bars.append((name[b.symbol], b.ts, p.high, p.low, p.close, p.volume))
        elif b.kind == funding:
            prints.append((name[b.symbol], b.ts, e.payload.funding_rate))
        return None

    bt.set_on_event(on_event)
    bt.plan()
    bt.run()
    stats = bt.fetch_stats()
    if stats.completed_failed:
        raise RuntimeError(f"{stats.completed_failed} fetches failed")
    bars = pd.DataFrame(bars, columns=["symbol", "close_time", "high", "low", "close", "volume"])
    bars["day"] = pd.to_datetime(bars.close_time, unit="ns", utc=True).dt.floor("D")
    prints = pd.DataFrame(prints, columns=["symbol", "ts", "rate"])
    prints["t"] = pd.to_datetime(prints.ts, unit="ns", utc=True)
    return bars, prints, bt.reports()


def _runs(days):
    """Contiguous runs of a sorted day index, as (first, last) pairs."""
    runs = []
    for d in days:
        if runs and d - runs[-1][1] == pd.Timedelta(days=1):
            runs[-1][1] = d
        else:
            runs.append([d, d])
    return runs


def daily_bars(symbols, start, end, log=print):
    """Daily high, low, close and volume in long form, for days in [start, end)."""
    q = module()
    spec = [q.StreamSpec(q.EndpointKind.Klines, "1d")]
    bars, _, reports = _collect(symbols, spec, q.Cadence.Monthly, start, end)

    fills = []
    for sym, g in bars.groupby("symbol"):
        span = pd.date_range(g.day.min(), g.day.max(), freq="D")
        for first, last in _runs(span.difference(g.day)):
            filled, _, _ = _collect([sym], spec, q.Cadence.Daily, first, last + pd.Timedelta(days=1))
            fills.append(filled)
            log(f"{sym} filled {first.date()} to {last.date()} from daily files, {len(filled)} rows")
    if fills:
        bars = pd.concat([bars, *fills]).drop_duplicates(["symbol", "day"])

    lo, hi = pd.Timestamp(start, tz="UTC"), pd.Timestamp(end, tz="UTC")
    bars = bars[(bars.day >= lo) & (bars.day < hi)]
    return bars.drop(columns="close_time").sort_values(["symbol", "day"]).reset_index(drop=True), reports


def daily_closes(symbols, start, end, log=print):
    """Daily close per symbol for days in [start, end), symbols on columns."""
    bars, reports = daily_bars(symbols, start, end, log)
    return bars.pivot(index="day", columns="symbol", values="close").sort_index(), reports


def funding_prints(symbols, start, end):
    """Funding prints in long form for stamps in [start, end). Positive means longs pay."""
    q = module()
    _, prints, reports = _collect(symbols, [q.StreamSpec(q.EndpointKind.FundingRate)], q.Cadence.Monthly, start, end)
    lo, hi = pd.Timestamp(start, tz="UTC"), pd.Timestamp(end, tz="UTC")
    prints = prints[(prints.t >= lo) & (prints.t < hi)]
    return prints.drop(columns="ts").sort_values(["symbol", "t"]).reset_index(drop=True), reports


def funding(symbols, start, end):
    """Funding prints per symbol for stamps in [start, end), symbols on columns. Positive means longs pay."""
    prints, reports = funding_prints(symbols, start, end)
    return prints.pivot_table(index="t", columns="symbol", values="rate", aggfunc="last").sort_index(), reports


def universe(start, end, chunk=50, log=print):
    """Daily bars and funding for every USDT perp, pulled in chunks and cached.

    A chunk already on disk is read back rather than streamed, so an interrupted
    pull resumes where it stopped.
    """
    tag = f"{pd.Timestamp(start).date()}_{pd.Timestamp(end).date()}"
    folder = CACHE / f"usdm_daily_{tag}"
    folder.mkdir(parents=True, exist_ok=True)
    # The coin list is fixed with the cache, so a later listing cannot shift the chunks.
    listed = folder / "symbols.txt"
    if not listed.exists():
        listed.write_text("\n".join(usdm_perps()))
    symbols = listed.read_text().split()

    bars, prints = [], []
    for i in range(0, len(symbols), chunk):
        part = symbols[i:i + chunk]
        bars_file, prints_file = folder / f"bars_{i:04d}.parquet", folder / f"funding_{i:04d}.parquet"
        if not (bars_file.exists() and prints_file.exists()):
            b, _ = daily_bars(part, start, end, log)
            p, _ = funding_prints(part, start, end)
            b.to_parquet(bars_file)
            p.to_parquet(prints_file)
            log(f"chunk {i // chunk + 1} of {-(-len(symbols) // chunk)} streamed, {len(b)} bars")
        bars.append(pd.read_parquet(bars_file))
        prints.append(pd.read_parquet(prints_file))
    return pd.concat(bars, ignore_index=True), pd.concat(prints, ignore_index=True)


def open_interest(symbols, start, end, log=print):
    """Open interest in quote terms per 5-minute sample, symbols on columns, cached."""
    path = CACHE / f"open_interest_{'_'.join(symbols)}_{pd.Timestamp(start).date()}_{pd.Timestamp(end).date()}.parquet"
    if path.exists():
        return pd.read_parquet(path)
    q = module()
    um = int(q.BinanceMarket.UsdM)
    builder = q.SubscriptionBuilder()
    for s in symbols:
        builder.add(q.ExchangeId.Binance, um, s)
    sub = builder.build()
    name = {sub.resolve(q.ExchangeId.Binance, um, s).symbol: s for s in symbols}
    pool = q.FetchPoolConfig()
    pool.workers = 32
    cfg = q.BinanceHistoricalConfig([q.StreamSpec(q.EndpointKind.Metrics)], q.Cadence.Daily, _ns(start), _ns(end) - 1, pool)
    src = q.BinanceHistoricalSource(sub, cfg)
    src.plan()
    rows = [(name[e.base.symbol], e.base.ts, e.payload.open_interest_value) for e in src]
    failed = src.fetch_stats().completed_failed
    if failed:
        log(f"{failed} metrics files failed")
    out = pd.DataFrame(rows, columns=["symbol", "ts", "oi_value"])
    out["t"] = pd.to_datetime(out.ts, unit="ns", utc=True)
    out = out.pivot_table(index="t", columns="symbol", values="oi_value", aggfunc="last").sort_index()
    out.to_parquet(path)
    return out
