"""Clean weekly panels of every USDT perp, built from the cached stream.

A day counts only if something traded. A signal return needs the day before to
have traded too, so no signal return spans a halt. Each coin's first 30 trading
days are skipped. A week needs 5 valid days. Liquidity is the 30-day median
dollar volume, taken as of the end of the prior week so it is known at trade
time. Non-crypto perps, stocks, ETFs, commodities and tokenised real assets,
are dropped by default.

PnL returns do span halts. A position cannot be closed while its coin is
halted, so the gap books on the day trading resumes. Redenominations are the
exception, listed by symbol and reopening day, since their gap is not a move.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import pandas as pd

import stream

# Symbol and the day trading resumed at the new denomination.
REDENOMINATIONS = {('BNXUSDT', '2023-02-22'), ('VENUSDT', '2018-10-19')}


@dataclass
class Panel:
    weekly: pd.DataFrame       # weekly log return, week ending Sunday
    simple: pd.DataFrame       # weekly simple PnL return from any traded days, gaps across halts included
    liquidity: pd.DataFrame    # 30-day median dollar volume known before the week
    vol: pd.DataFrame          # weekly volatility over the prior 12 weeks
    funding: pd.DataFrame      # funding summed over the week, positive means longs pay
    halted: pd.DataFrame       # true where the coin could not trade at the rebalance opening the week
    gaps: pd.DataFrame         # weekly log return booked across halts, for reporting


def load(start='2020-01-01', end='2026-09-01', crypto_only=True) -> Panel:
    bars, prints = stream.universe(start, end)
    if crypto_only:
        drop = stream.non_crypto()
        bars, prints = bars[~bars.symbol.isin(drop)], prints[~prints.symbol.isin(drop)]
    return build(bars, prints)


def build(bars: pd.DataFrame, prints: pd.DataFrame | None = None) -> Panel:
    """The weekly panel from daily bars and funding prints in long form. No prints means no funding."""
    close = bars.pivot(index='day', columns='symbol', values='close')
    volume = bars.pivot(index='day', columns='symbol', values='volume')

    traded = volume > 0
    dollar_volume = (volume * close).where(traded)
    daily = np.log(close.where(traded)).diff().where(traded & traded.shift(1, fill_value=False))
    daily = daily.where(traded.cumsum().where(traded) > 30)

    count = daily.notna().resample('W-SUN').sum()
    weekly = daily.resample('W-SUN').sum(min_count=5).where(count >= 5)
    liquidity = dollar_volume.rolling(30, min_periods=20).median().resample('W-SUN').last().shift(1)
    vol = weekly.rolling(12, min_periods=8).std().shift(1)

    if prints is None or prints.empty:
        funding = pd.DataFrame(0.0, index=weekly.index, columns=weekly.columns)
    else:
        fund = prints.pivot_table(index='t', columns='symbol', values='rate', aggfunc='last')
        funding = fund.resample('W-SUN').sum().reindex(index=weekly.index, columns=weekly.columns).fillna(0.0)

    held = np.log(close.where(traded)).ffill().diff().where(traded)
    reopen = traded & ~traded.shift(1, fill_value=False) & held.notna()
    for sym, day in REDENOMINATIONS:
        day = pd.Timestamp(day, tz='UTC')
        if sym in held.columns and day in held.index:
            held.loc[day, sym] = np.nan
    gaps = held.where(reopen).resample('W-SUN').sum(min_count=1)
    simple = np.expm1(held.resample('W-SUN').sum(min_count=1))

    # A coin idle on the week's last day that trades again later is halted, not delisted.
    trades_later = traded[::-1].cummax()[::-1]
    halted = (~traded.resample('W-SUN').last() & trades_later.resample('W-SUN').last()).shift(1, fill_value=False)
    return Panel(weekly=weekly, simple=simple, liquidity=liquidity.reindex(weekly.index),
                 vol=vol, funding=funding, halted=halted.reindex(weekly.index).fillna(False).astype(bool),
                 gaps=gaps.reindex(weekly.index))
