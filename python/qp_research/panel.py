"""Clean panels of returns, liquidity, volatility and funding at any rebalance period.

A bar counts only if something traded. A signal return needs the bar before to
have traded too, so no signal return spans a halt, and each coin's first bars
are skipped. A period needs a minimum of valid bars. Liquidity is the rolling
median dollar volume and volatility the rolling spread of period returns, both
as of the end of the prior period so they are known at trade time.

PnL returns do span halts. A position cannot be closed while its coin is
halted, so the gap books on the bar trading resumes. Redenominations are the
exception, since their gap is not a move.

A funding print belongs to the bar it falls in, so it is paid by the position
held over that bar.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import pandas as pd

from . import data
from .data import universe


@dataclass
class Bars:
    """Bar-level panels, symbols on columns, shared by every period built from them."""
    close: pd.DataFrame
    traded: pd.DataFrame          # true where the bar has volume
    dollar_volume: pd.DataFrame   # volume times close on traded bars
    returns: pd.DataFrame         # log return on traded bars after a traded bar, warmup skipped
    held: pd.DataFrame            # log return a held position books, halt gaps included
    funding: pd.DataFrame         # funding paid per bar, positive means longs pay


@dataclass
class Panel:
    """Period-level panels, periods on rows and symbols on columns."""
    period: str                   # pandas rule naming where each period ends
    returns: pd.DataFrame         # log return over the period, from valid bars only
    pnl: pd.DataFrame             # simple return a held position books, gaps across halts included
    liquidity: pd.DataFrame       # rolling median dollar volume known before the period
    vol: pd.DataFrame             # spread of period returns over prior periods
    funding: pd.DataFrame         # funding summed over the period
    halted: pd.DataFrame          # true where the coin could not trade at the rebalance opening the period
    gaps: pd.DataFrame            # log return booked across halts, for reporting
    bars: Bars


def bars(long: pd.DataFrame, prints: pd.DataFrame | None = None, at='day', warmup=30) -> Bars:
    """Bar-level panels from bars in long form, and funding prints in long form when given.

    at names the bar time column. Prints are placed on the bar they fall in.
    """
    close = long.pivot(index=at, columns='symbol', values='close').sort_index()
    volume = long.pivot(index=at, columns='symbol', values='volume').reindex_like(close)
    traded = volume > 0
    level = np.log(close.where(traded))
    returns = level.diff().where(traded & traded.shift(1, fill_value=False))
    returns = returns.where(traded.cumsum().where(traded) > warmup)

    held = level.ffill().diff().where(traded)
    for sym, day in universe.REDENOMINATIONS:
        day = pd.Timestamp(day, tz='UTC')
        if sym in held.columns and day in held.index:
            held.loc[day, sym] = np.nan

    funding = pd.DataFrame(0.0, index=close.index, columns=close.columns)
    if prints is not None and len(prints):
        step = pd.Series(close.index).diff().median()
        prints = prints.drop_duplicates(['symbol', 't'], keep='last')
        per_bar = prints.assign(bar=prints.t.dt.floor(step)).pivot_table(index='bar', columns='symbol', values='funding_rate', aggfunc='sum')
        funding = per_bar.reindex(index=close.index, columns=close.columns).fillna(0.0)
    return Bars(close=close, traded=traded, dollar_volume=(volume * close).where(traded), returns=returns,
                held=held, funding=funding)


def build(b: Bars, period='W-SUN', min_bars=5, liquidity_bars=30, liquidity_min=20, vol_periods=12, vol_min=8) -> Panel:
    """The panel at one rebalance period from bar-level panels."""
    count = b.returns.notna().resample(period).sum()
    returns = b.returns.resample(period).sum(min_count=min_bars).where(count >= min_bars)
    liquidity = b.dollar_volume.rolling(liquidity_bars, min_periods=liquidity_min).median().resample(period).last().shift(1)
    vol = returns.rolling(vol_periods, min_periods=vol_min).std().shift(1)
    funding = b.funding.resample(period).sum().reindex_like(returns).fillna(0.0)

    reopen = b.traded & ~b.traded.shift(1, fill_value=False) & b.held.notna()
    gaps = b.held.where(reopen).resample(period).sum(min_count=1)
    pnl = np.expm1(b.held.resample(period).sum(min_count=1))

    # A coin idle on the period's last bar that trades again later is halted, not delisted.
    trades_later = b.traded[::-1].cummax()[::-1]
    halted = (~b.traded.resample(period).last() & trades_later.resample(period).last()).shift(1, fill_value=False)
    return Panel(period=period, returns=returns, pnl=pnl.reindex_like(returns), liquidity=liquidity.reindex(returns.index),
                 vol=vol, funding=funding, halted=halted.reindex(returns.index).fillna(False).astype(bool),
                 gaps=gaps.reindex(returns.index), bars=b)


def load(start='2020-01-01', end='2026-09-01', period='W-SUN', market='usdm', crypto_only=True, symbols=None, **kw) -> Panel:
    """The panel of a market's whole listed universe, from cached daily bars and funding."""
    if symbols is None:
        symbols = universe.perps(crypto_only) if market == 'usdm' else universe.spot_pairs()
    daily = data.daily_bars(symbols, start, end, market)
    prints = data.funding(symbols, start, end) if market == 'usdm' else None
    return build(bars(daily, prints), period, **kw)
