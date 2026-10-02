"""Loaders for the 10 Binance USD-M perps on disk.

Daily close per symbol from 1m klines, weekly resample, and the per-symbol
round-trip cost from the cost table. Daily panel is cached as parquet so the
notebook re-execute skips the ~11k CSV read.
"""
from __future__ import annotations

import glob
from pathlib import Path

import numpy as np
import pandas as pd


DATA = Path("/home/danie/quant-data/binance_historical")
COST = Path("/home/danie/quant-data/cost_model/cost_table.csv")
CACHE = Path("/home/danie/quant-data/derived/binance_perp_daily_close.parquet")
FUNDING_CACHE = Path("/home/danie/quant-data/derived/binance_perp_funding.parquet")

def _has_1m_coverage(sym: str) -> bool:
    return bool(glob.glob(str(DATA / sym / "futures/klines" / f"{sym}-1m-*.csv")))


SYMBOLS = sorted(p.name for p in DATA.iterdir() if p.is_dir() and _has_1m_coverage(p.name))

_KLINE_COLS = [
    "symbol", "kind", "open_ts", "open", "high", "low", "close", "volume",
    "close_ts", "quote_volume", "num_trades", "taker_buy_base",
    "taker_buy_quote", "ignore",
]


def _load_daily_close_one(symbol: str) -> pd.Series:
    """Daily close from the last 1-minute bar of each daily kline file."""
    files = sorted(glob.glob(str(DATA / symbol / "futures/klines" / f"{symbol}-1m-*.csv")))
    rows = []
    for f in files:
        df = pd.read_csv(f, header=None, names=_KLINE_COLS, usecols=[8, 6])
        if len(df):
            r = df.iloc[-1]
            rows.append((r.close_ts, r.close))
    if not rows:
        return pd.Series(dtype=float, name=symbol)
    ts = pd.to_datetime([r[0] for r in rows], unit="ms", utc=True).floor("D")
    s = pd.Series([r[1] for r in rows], index=ts).astype(float)
    s = s[~s.index.duplicated(keep="last")].sort_index()
    return s.rename(symbol)


def load_daily_panel(rebuild: bool = False) -> pd.DataFrame:
    """Panel of daily close, symbols on columns. Cached to parquet."""
    if not rebuild and CACHE.exists():
        return pd.read_parquet(CACHE)
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    panel = pd.DataFrame({s: _load_daily_close_one(s) for s in SYMBOLS}).sort_index()
    panel.to_parquet(CACHE)
    return panel


def weekly_close(daily: pd.DataFrame) -> pd.DataFrame:
    """Weekly close (Sunday label, last-of-week)."""
    return daily.resample("W-SUN").last()


def cost_round_trip_bps() -> pd.Series:
    """Per-symbol median round-trip cost in bps, from the cost table."""
    c = pd.read_csv(COST)
    g = c.groupby("symbol").agg(hs=("half_spread_bps", "median"),
                                fee=("taker_fee_bps", "median"))
    return (2.0 * (g.hs + g.fee)).rename("round_trip_bps")


def _load_funding_one(symbol: str) -> pd.Series:
    """8-hour funding print series for one symbol."""
    files = sorted(glob.glob(str(DATA / symbol / "futures/funding" / f"{symbol}-fundingRate-*.csv")))
    frames = []
    for f in files:
        df = pd.read_csv(f, header=None, names=["symbol","kind","ts_ms","hour","rate"],
                         usecols=[2, 4])
        frames.append(df)
    if not frames:
        return pd.Series(dtype=float, name=symbol)
    df = pd.concat(frames, ignore_index=True)
    ts = pd.to_datetime(df["ts_ms"], unit="ms", utc=True)
    s = pd.Series(df["rate"].astype(float).values, index=ts).sort_index()
    s = s[~s.index.duplicated(keep="last")]
    return s.rename(symbol)


def load_funding_panel(rebuild: bool = False) -> pd.DataFrame:
    """Panel of 8h funding prints, symbols on columns. Positive value = longs pay."""
    if not rebuild and FUNDING_CACHE.exists():
        return pd.read_parquet(FUNDING_CACHE)
    FUNDING_CACHE.parent.mkdir(parents=True, exist_ok=True)
    panel = pd.DataFrame({s: _load_funding_one(s) for s in SYMBOLS}).sort_index()
    panel.to_parquet(FUNDING_CACHE)
    return panel


def weekly_funding_sum(funding: pd.DataFrame, label: str = "W-SUN") -> pd.DataFrame:
    """Sum of funding prints per week. Row t is the sum over the week ending at ts t.
    Long-position PnL contribution over the week is minus this value."""
    return funding.resample(label).sum()
