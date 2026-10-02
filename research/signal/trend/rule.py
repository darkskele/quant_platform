"""The plain sign and the three-part trend rule as sliced books, on any daily panel.

A line is fitted through each coin's log price over the trailing window. The
plain sign trades every coin on the sign of its trailing return. The strict
rule trades a coin on the sign of the fitted slope, only where the line is
steep enough and fits tightly enough. A sliced book splits its capital evenly
over one book per rebalance day, so no day of the week decides the result.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import pandas as pd

Z_CUT, R2_CUT = 2.0, 0.8


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


def parts(b, ln: Lines, week: str, days: int, per_week: int):
    """Sign of the trailing return, sign of the slope, steepness and fit at each rebalance close.

    Steepness is the fitted move over the window in units of the coin's own
    volatility over that long. per_week is the number of daily bars in a week.
    """
    at = lambda x: x.resample(week).last().reindex(b.W.index)
    ret, slope = at(ln.trail), at(ln.slope)
    ok = (b.LIQ.shift(-1) >= 1e7) & b.VOL.shift(-1).notna() & ret.notna()
    z = (slope.abs() * days / (b.VOL.shift(-1) * np.sqrt(days / per_week))).where(ok)
    return np.sign(ret), np.sign(slope), z, at(ln.corr) ** 2


def plain(b, ln: Lines, week: str, days: int, per_week: int) -> pd.DataFrame:
    """Every liquid coin on the sign of its trailing return."""
    return b.sized(parts(b, ln, week, days, per_week)[0].shift(1))


def strict(b, ln: Lines, week: str, days: int, per_week: int, z_cut=Z_CUT, r2_cut=R2_CUT) -> pd.DataFrame:
    """Coins whose line clears both cuts, on the sign of the slope, fully invested in those that pass."""
    _, direction, z, r2 = parts(b, ln, week, days, per_week)
    return b.sized(direction.where((z >= z_cut) & (r2 >= r2_cut)).shift(1))


def slice_daily(b, w: pd.DataFrame, held: pd.DataFrame, **cost) -> pd.Series:
    """One slice's net return by day. Price moves land on their day, funding and cost on the week's last day."""
    r = b.run(w, **cost)
    by_day = b.hold(w).reindex(held.index, method='bfill').fillna(0.0)
    since_rebalance = held.groupby(b.W.index.searchsorted(held.index)).cumsum()
    gross = (by_day * (np.exp(since_rebalance) - np.exp(since_rebalance - held))).sum(axis=1)
    return gross.add((r.funding + r.cost).reindex(held.index).fillna(0.0), fill_value=0.0)


def sliced(books: dict, weights, held: pd.DataFrame, **cost) -> pd.Series:
    """Weekly net return of the book holding one equal slice per rebalance day.

    books maps each week rule to its Book. weights(b, week) gives a slice's weights.
    """
    parts_ = [slice_daily(b, weights(b, week), held, **cost) for week, b in books.items()]
    weekly = pd.concat(parts_, axis=1).mean(axis=1).resample('W-SUN').sum()
    return weekly.loc[(weekly != 0).idxmax():]


def pair(a: pd.Series, b: pd.Series, window=52, least=26) -> pd.Series:
    """Two books held at equal risk, each scaled by its own volatility over the prior weeks."""
    a, b = a.align(b, join='inner')
    scale = lambda x: 1.0 / x.rolling(window, min_periods=least).std().shift(1)
    return ((a * scale(a) + b * scale(b)) / 2).dropna()
