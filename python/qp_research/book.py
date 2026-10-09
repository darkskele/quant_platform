"""Books turn weights into net returns after funding and cost.

A weight set at a period's close earns the next period's return and pays that
period's funding. Cost is charged on every change of weight, one way, by the
book's cost model. A coin halted at the rebalance keeps its weight until it
trades again.

A sliced book splits its capital evenly over one book per rebalance day, so no
day decides the result. Each slice books its moves by bar, its funding and cost
on its own rebalance bar, and the slices are averaged and summed to the
reporting period.
"""
from __future__ import annotations

from typing import Callable

import numpy as np
import pandas as pd

from . import panel as panel_
from .costs import Flat, Provisional, Taker

CostModel = Flat | Provisional | Taker
WEEK = ('W-MON', 'W-TUE', 'W-WED', 'W-THU', 'W-FRI', 'W-SAT', 'W-SUN')


def inverse_vol(p: panel_.Panel, signal: pd.DataFrame, floor=1e7, hi=np.inf) -> pd.DataFrame:
    """Weights in the direction of signal, scaled by inverse volatility over coins in the liquidity band, absolute sum one."""
    elig = (p.liquidity >= floor) & (p.liquidity < hi) & p.vol.notna() & (p.vol > 0) & signal.notna()
    raw = (signal / p.vol).where(elig)
    return raw.div(raw.abs().sum(axis=1), axis=0)


def gated(w: pd.DataFrame, on: pd.Series) -> pd.DataFrame:
    """The weights where the condition is on, flat elsewhere."""
    return w.where(on.reindex(w.index).fillna(False).astype(bool), 0.0)


class Book:
    """Weights in, net period returns out, on one panel priced by one cost model."""

    def __init__(self, p: panel_.Panel, cost: CostModel, ledger=None):
        self.p, self.cost, self.ledger = p, cost, ledger
        self.R = p.pnl.reindex_like(p.returns).fillna(0.0)

    def hold(self, w: pd.DataFrame) -> pd.DataFrame:
        """Carries each halted coin's weight from the period before."""
        w = w.reindex_like(self.p.returns).fillna(0.0)
        a, h = w.values.copy(), self.p.halted.reindex_like(w).fillna(False).values
        for i in range(1, len(a)):
            a[i, h[i]] = a[i - 1, h[i]]
        return pd.DataFrame(a, index=w.index, columns=w.columns)

    def run(self, w: pd.DataFrame, cost_mult=1.0, name=None, kind='exploratory', params=None, record=True) -> pd.DataFrame:
        """Gross, funding, cost, turnover and net per period, from the first period holding anything.

        With a ledger, the net returns are recorded as a trial unless record is
        False. A run at other than full cost is recorded as a check.
        """
        w = self.hold(w)
        dw = (w - w.shift(1).fillna(0.0)).abs()
        bps = self.cost.bps(self.p.liquidity, dw)
        out = pd.DataFrame({'gross': (w * self.R).sum(axis=1), 'funding': -(w * self.p.funding).sum(axis=1),
                            'cost': -(dw * bps * cost_mult / 1e4).sum(axis=1), 'turnover': dw.sum(axis=1)})
        out['net'] = out.gross + out.funding + out.cost
        out = out.loc[(w.abs().sum(axis=1) > 0).idxmax():]
        if self.ledger is not None and record:
            self.ledger.record(out.net, name, 'check' if cost_mult != 1.0 else kind, params)
        return out

    def by_bar(self, w: pd.DataFrame, cost_mult=1.0) -> pd.DataFrame:
        """Gross by bar as weights drift within each period, funding, cost and turnover on the period's last bar."""
        r = self.run(w, cost_mult, record=False)
        held = self.p.bars.held.fillna(0.0)
        by_bar = self.hold(w).reindex(held.index, method='bfill').fillna(0.0)
        since = held.groupby(self.p.returns.index.searchsorted(held.index)).cumsum()
        gross = (by_bar * (np.exp(since) - np.exp(since - held))).sum(axis=1)
        # Booked on each period's last bar, which falls short of its label when the data ends mid period.
        last = held.index[held.index.searchsorted(r.index, side='right') - 1]
        out = r[['funding', 'cost', 'turnover']].set_axis(last).groupby(level=0).sum().reindex(held.index).fillna(0.0)
        out.insert(0, 'gross', gross)
        out['net'] = out.gross + out.funding + out.cost
        return out

    def sleeve(self, w: pd.DataFrame, on: pd.Series, cost_mult=1.0, name=None, kind='exploratory', params=None) -> pd.Series:
        """Net returns on the periods the condition is on, entry and exit cost included, recorded as one trial."""
        r = self.run(gated(w, on), cost_mult, record=False)
        m = on.reindex(r.index).fillna(False).astype(bool)
        # Exit cost lands the period after the gate turns off, so book it to the last on period.
        exits = ~m & m.shift(1, fill_value=False)
        out = (r.net + r.cost.where(exits, 0.0).shift(-1).fillna(0.0))[m]
        if self.ledger is not None:
            self.ledger.record(out, name, 'check' if cost_mult != 1.0 else kind, params)
        return out


class Sliced:
    """One equal slice per rebalance period, each a book on its own panel from shared bars."""

    def __init__(self, bars: panel_.Bars, cost: CostModel, periods=WEEK, report='W-SUN', ledger=None, **panel_kw):
        self.books = {rule: Book(panel_.build(bars, rule, **panel_kw), cost) for rule in periods}
        self.report, self.ledger = report, ledger

    def run(self, weights: Callable[[panel_.Panel], pd.DataFrame], cost_mult=1.0, name=None, kind='exploratory', params=None) -> pd.DataFrame:
        """Gross, funding, cost, turnover and net per reporting period, from the first period with a return.

        weights(panel) gives a slice's weights on its own panel. With a ledger
        the whole book's net returns are one trial.
        """
        parts = [b.by_bar(weights(b.p), cost_mult) for b in self.books.values()]
        out = pd.concat(parts).groupby(level=0).sum() / len(parts)
        out = out.resample(self.report).sum()
        out = out.loc[(out.net != 0).idxmax():]
        if self.ledger is not None:
            self.ledger.record(out.net, name, 'check' if cost_mult != 1.0 else kind, params)
        return out


def equal_risk(a: pd.Series, b: pd.Series, window=52, least=26) -> pd.Series:
    """Two books held at equal risk, each scaled by its own volatility over prior periods."""
    a, b = a.align(b, join='inner')
    scale = lambda x: 1.0 / x.rolling(window, min_periods=least).std().shift(1)
    return ((a * scale(a) + b * scale(b)) / 2).dropna()
