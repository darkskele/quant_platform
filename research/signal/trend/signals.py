"""Trend signals as weights on qp_research panels.

The per coin book trades every liquid coin on the sign of its own trailing
return. The market book holds the basket all long or all short on the sign of
the average coin's trailing return. Both size by inverse volatility.

A line is fitted through each coin's log price over a trailing window of days.
The plain sign trades on the sign of the trailing return. The strict rule
trades on the sign of the fitted slope, only where the line is steep enough
and fits tightly enough.

Every book in this lane is weekly, so its Sharpe is annualised over 52 weeks.
"""
from __future__ import annotations

from dataclasses import dataclass
from functools import partial

import numpy as np
import pandas as pd

from qp_research import condition, data, stats
from qp_research.book import inverse_vol
from qp_research.panel import Panel

sharpe = partial(stats.sharpe, periods=52)

Z_CUT, R2_CUT = 2.0, 0.8


def signal(p: Panel, L: int) -> pd.DataFrame:
    """Sign of the trailing L-period return, known at the close before the period it trades."""
    return np.sign(p.returns.rolling(L, min_periods=L).sum()).shift(1)


def per_coin(p: Panel, L: int, floor=1e7) -> pd.DataFrame:
    return inverse_vol(p, signal(p, L), floor)


def basket(p: Panel, floor=1e7) -> pd.DataFrame:
    """Every liquid coin long at inverse volatility."""
    return inverse_vol(p, pd.DataFrame(1.0, index=p.returns.index, columns=p.returns.columns).where(p.returns.shift(1).notna()), floor)


def market(p: Panel, L: int, floor=1e7) -> pd.DataFrame:
    """The basket all long or all short on the sign of the average coin's trailing return."""
    m = condition.market_return(p, floor)
    return basket(p, floor).mul(np.sign(m.rolling(L, min_periods=L).sum()).shift(1), axis=0)


@dataclass
class Lines:
    trail: pd.DataFrame    # trailing log return over the window
    corr: pd.DataFrame     # correlation of log price with time over the window
    slope: pd.DataFrame    # fitted slope in log price a day


def lines(daily: pd.DataFrame, days: int) -> Lines:
    """The fitted line per coin and day over the trailing days, from daily log returns."""
    need = int(np.ceil(days * 5 / 7))
    level = daily.fillna(0.0).cumsum()
    clock = pd.Series(np.arange(len(level), dtype=float), index=level.index)
    enough = daily.notna().rolling(days).sum() >= need
    corr = level.rolling(days).corr(clock).where(enough)
    slope = level.rolling(days).cov(clock).div(clock.rolling(days).var(), axis=0).where(enough)
    return Lines(trail=daily.rolling(days, min_periods=need).sum(), corr=corr, slope=slope)


def parts(p: Panel, ln: Lines, days: int, per_week: int):
    """Sign of the trailing return, sign of the slope, steepness and fit at each rebalance close.

    Steepness is the fitted move over the window in units of the coin's own
    volatility over that long. per_week is the number of daily bars in a week.
    """
    at = lambda x: x.resample(p.period).last().reindex(p.returns.index)
    ret, slope = at(ln.trail), at(ln.slope)
    ok = (p.liquidity.shift(-1) >= 1e7) & p.vol.shift(-1).notna() & ret.notna()
    z = (slope.abs() * days / (p.vol.shift(-1) * np.sqrt(days / per_week))).where(ok)
    return np.sign(ret), np.sign(slope), z, at(ln.corr) ** 2


def plain(p: Panel, ln: Lines, days: int, per_week: int) -> pd.DataFrame:
    """Every liquid coin on the sign of its trailing return."""
    return inverse_vol(p, parts(p, ln, days, per_week)[0].shift(1))


def strict(p: Panel, ln: Lines, days: int, per_week: int, z_cut=Z_CUT, r2_cut=R2_CUT) -> pd.DataFrame:
    """Coins whose line clears both cuts, on the sign of the slope, fully invested in those that pass."""
    _, direction, z, r2 = parts(p, ln, days, per_week)
    return inverse_vol(p, direction.where((z >= z_cut) & (r2 >= r2_cut)).shift(1))


def open_interest(symbols, start, end) -> pd.DataFrame:
    """Open interest in quote terms per 5-minute sample, symbols on columns."""
    m = data.metrics(symbols, start, end)
    return m.pivot_table(index='t', columns='symbol', values='open_interest_value', aggfunc='last').sort_index()
