"""Market conditions and in-fold cuts on them, built from earlier periods only.

A condition is a series per period known before the period trades. Cut points
come from the condition's own history before each period, so a condition never
knows where it will sit in the full sample.
"""
from __future__ import annotations

import numpy as np
import pandas as pd

from . import gate, stats
from .book import Book, gated
from .panel import Panel

NAMES = {0: 'low', 1: 'mid', 2: 'high'}


def market_return(p: Panel, floor=1e7) -> pd.Series:
    """The average liquid coin's period return."""
    return p.returns.where(p.liquidity >= floor).mean(axis=1)


def strength(p: Panel, n_ret=4, n_vol=12, floor=1e7) -> pd.Series:
    """The market's trailing move in units of its own volatility, known before the period."""
    m = market_return(p, floor)
    return (m.rolling(n_ret).sum().abs() / (m.rolling(n_vol, min_periods=8).std() * np.sqrt(n_ret))).shift(1)


def market_conditions(p: Panel, floor=1e7) -> pd.DataFrame:
    """Six market conditions per period, each built from earlier periods only."""
    liquid = p.liquidity >= floor
    m = market_return(p, floor)
    z = (p.returns / p.vol).where(liquid)
    comove = pd.Series(np.nan, index=p.returns.index)
    for i in range(12, len(z)):
        blk = z.iloc[i - 12:i]
        blk = blk.loc[:, blk.notna().sum() >= 10]
        if blk.shape[1] >= 10:
            c = blk.corr().values
            n = c.shape[0]
            comove.iloc[i] = (np.nansum(c) - n) / (n * (n - 1))
    return pd.DataFrame({
        'co-movement': comove,
        'market vol': m.rolling(12, min_periods=8).std().shift(1),
        'trend strength': strength(p, floor=floor),
        'funding': p.funding.where(liquid).mean(axis=1).rolling(4).mean().shift(1),
        'dispersion': p.returns.where(liquid).rolling(4, min_periods=4).sum().std(axis=1).shift(1),
        'market direction': np.sign(m.rolling(4).sum()).shift(1)})


def _past_quantile(x: pd.Series, q: float, min_hist: int) -> pd.Series:
    """The q quantile of x over the periods before each one, NaN until min_hist values exist."""
    return x.shift(1).expanding(min_periods=min_hist).quantile(q)


def thirds(x: pd.Series, min_hist=52) -> pd.Series:
    """0, 1 or 2 for the low, mid or high third of x against earlier periods only."""
    lo, hi = _past_quantile(x, 1 / 3, min_hist), _past_quantile(x, 2 / 3, min_hist)
    out = pd.Series(np.where(x <= lo, 0.0, np.where(x > hi, 2.0, 1.0)), index=x.index)
    return out.where(lo.notna() & x.notna())


def past_cut(x: pd.Series, q=1 / 3, min_hist=52, below=True) -> pd.Series:
    """1 where x sits at or below its q quantile over earlier periods, above it when below is False. NaN before min_hist."""
    cut = _past_quantile(x, q, min_hist)
    hit = (x <= cut) if below else (x > cut)
    return hit.astype(float).where(cut.notna() & x.notna())


def labels(cond: pd.DataFrame, idx: pd.Index) -> dict:
    """Each condition's third per period. Market direction is 0 after a fall and 2 after a rise."""
    return {k: (thirds(v) if k != 'market direction' else v.map({-1.0: 0, 1.0: 2})).reindex(idx) for k, v in cond.items()}


def cells(cond: pd.DataFrame, idx: pd.Index) -> dict:
    """On mask for every condition third, named like 'trend strength low'."""
    return {f'{k} {NAMES[v]}': lab == v for k, lab in labels(cond, idx).items() for v in (0, 1, 2) if (lab == v).any()}


def walk_forward(b: Book, w: pd.DataFrame, masks: dict, folds=4, history=156, min_on=20):
    """Picks a condition in each fold from earlier periods only, then trades its gated book.

    masks maps a name to its on mask. At each fold start the cell whose gated
    book had the best Sharpe on its on periods before it, among cells on for at
    least min_on of those periods, is traded through the fold. Returns the
    stitched gated returns, the on mask they traded under, and the cell picked
    per fold.
    """
    idx = b.run(w).index
    books = pd.DataFrame({n: b.run(gated(w, on)).net.reindex(idx) for n, on in masks.items()}).fillna(0.0)
    on = {n: m.reindex(idx).fillna(False).astype(bool) for n, m in masks.items()}

    def pick(past):
        def score(n):
            live = on[n].reindex(past.index)
            return stats.sharpe(past[n][live], 1) if live.sum() >= min_on else -np.inf
        return max(on, key=score)

    oos, picked = gate.walk_forward(books, folds=folds, min_train=history, pick=pick)
    edges = np.linspace(history, len(books), folds + 1).astype(int)
    mask = pd.concat([on[n].iloc[lo:hi] for n, lo, hi in zip(picked, edges[:-1], edges[1:])])
    return oos, mask, dict(zip([books.index[lo] for lo in edges[:-1]], picked))
