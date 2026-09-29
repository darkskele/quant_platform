"""The weekly trend book on the cleaned panel, shared by every trend notebook.

A weight set at a Sunday close earns the next week's return and pays that
week's funding. Cost is charged on every change of weight, one way. A coin
halted at the rebalance keeps its weight until it trades again.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import costs  # noqa: E402
import gate  # noqa: E402
import panel  # noqa: E402

sharpe = gate.sharpe


class Book:
    """Weights in, net weekly returns out, on one loaded panel.

    Cost is the provisional flat rate unless a cost ladder and a book size are
    given, then each trade is priced off the ladder at its dollar size.
    """

    def __init__(self, p: panel.Panel | None = None, fee_bps=costs.FEE_BPS):
        self.p = p or panel.load()
        self.fee_bps = fee_bps
        self.W, self.LIQ, self.VOL, self.F = self.p.weekly, self.p.liquidity, self.p.vol, self.p.funding
        self.R = self.p.simple.reindex_like(self.W).fillna(0.0)
        self.provisional = pd.DataFrame(np.where(self.LIQ >= 1e7, 7.5, 15.0), index=self.LIQ.index, columns=self.LIQ.columns)

    def hold(self, w: pd.DataFrame) -> pd.DataFrame:
        """Carries each halted coin's weight from the week before."""
        w = w.reindex_like(self.W).fillna(0.0)
        a, h = w.values.copy(), self.p.halted.reindex_like(self.W).fillna(False).values
        for i in range(1, len(a)):
            a[i, h[i]] = a[i - 1, h[i]]
        return pd.DataFrame(a, index=w.index, columns=w.columns)

    def run(self, w: pd.DataFrame, cost_mult=1.0, fit=None, size=None) -> pd.DataFrame:
        """Gross, funding, cost, turnover and net per week, from the first week holding anything."""
        w = self.hold(w)
        dw = (w - w.shift(1).fillna(0.0)).abs()
        bps = self.provisional if size is None else costs.one_way_bps(fit, self.LIQ, dw * size, self.fee_bps)
        out = pd.DataFrame({'gross': (w * self.R).sum(axis=1), 'funding': -(w * self.F).sum(axis=1),
                            'cost': -(dw * bps * cost_mult / 1e4).sum(axis=1), 'turnover': dw.sum(axis=1)})
        out['net'] = out.gross + out.funding + out.cost
        return out.loc[(w.abs().sum(axis=1) > 0).idxmax():]

    def sized(self, sig: pd.DataFrame, floor=1e7, hi=np.inf) -> pd.DataFrame:
        """Inverse volatility weights on coins in the liquidity band, absolute weights summing to one."""
        elig = (self.LIQ >= floor) & (self.LIQ < hi) & self.VOL.notna() & (self.VOL > 0) & sig.notna()
        raw = (sig / self.VOL).where(elig)
        return raw.div(raw.abs().sum(axis=1), axis=0)

    def signal(self, L: int) -> pd.DataFrame:
        """Sign of the trailing L-week return, known at the close before the week it trades."""
        return np.sign(self.W.rolling(L, min_periods=L).sum()).shift(1)

    def per_coin(self, L: int, floor=1e7) -> pd.DataFrame:
        return self.sized(self.signal(L), floor)

    def basket(self, floor=1e7) -> pd.DataFrame:
        return self.sized(pd.DataFrame(1.0, index=self.W.index, columns=self.W.columns).where(self.W.shift(1).notna()), floor)

    def market_ret(self, floor=1e7) -> pd.Series:
        return self.W.where(self.LIQ >= floor).mean(axis=1)

    def market(self, L: int, floor=1e7) -> pd.DataFrame:
        """The basket all long or all short on the sign of the average coin's trailing return."""
        return self.basket(floor).mul(np.sign(self.market_ret(floor).rolling(L, min_periods=L).sum()).shift(1), axis=0)

    def strength(self, n_ret=4, n_vol=12, floor=1e7) -> pd.Series:
        """The market's trailing move in units of its own volatility, known before the week."""
        m = self.market_ret(floor)
        return (m.rolling(n_ret).sum().abs() / (m.rolling(n_vol, min_periods=8).std() * np.sqrt(n_ret))).shift(1)

    def conditions(self, floor=1e7) -> pd.DataFrame:
        """Six market conditions per week, each built from earlier weeks only."""
        liquid = self.LIQ >= floor
        m = self.market_ret(floor)
        z = (self.W / self.VOL).where(liquid)
        comove = pd.Series(np.nan, index=self.W.index)
        for i in range(12, len(self.W)):
            blk = z.iloc[i - 12:i]
            blk = blk.loc[:, blk.notna().sum() >= 10]
            if blk.shape[1] >= 10:
                c = blk.corr().values
                n = c.shape[0]
                comove.iloc[i] = (np.nansum(c) - n) / (n * (n - 1))
        return pd.DataFrame({
            'co-movement': comove,
            'market vol': m.rolling(12, min_periods=8).std().shift(1),
            'trend strength': self.strength(floor=floor),
            'funding': self.F.where(liquid).mean(axis=1).rolling(4).mean().shift(1),
            'dispersion': self.W.where(liquid).rolling(4, min_periods=4).sum().std(axis=1).shift(1),
            'market direction': np.sign(m.rolling(4).sum()).shift(1)})

    @staticmethod
    def thirds(x: pd.Series, min_hist=52) -> pd.Series:
        """0, 1 or 2 for the low, mid or high third of x against earlier weeks only."""
        out = pd.Series(np.nan, index=x.index)
        for i in range(len(x)):
            past = x.iloc[:i].dropna()
            if len(past) >= min_hist and not np.isnan(x.iloc[i]):
                lo, hi = past.quantile([1 / 3, 2 / 3])
                out.iloc[i] = 0 if x.iloc[i] <= lo else (2 if x.iloc[i] > hi else 1)
        return out

    @classmethod
    def labels(cls, cond: pd.DataFrame, idx: pd.Index) -> dict:
        """Each condition's third per week. Market direction is 0 after a fall and 2 after a rise."""
        return {k: (cls.thirds(v) if k != 'market direction' else v.map({-1.0: 0, 1.0: 2})).reindex(idx)
                for k, v in cond.items()}

    @classmethod
    def cells(cls, cond: pd.DataFrame, idx: pd.Index) -> dict:
        """On mask for every condition third, named like 'trend strength low'."""
        names = {0: 'low', 1: 'mid', 2: 'high'}
        return {f'{k} {names[v]}': lab == v for k, lab in cls.labels(cond, idx).items() for v in (0, 1, 2) if (lab == v).any()}

    @staticmethod
    def past_cut(x: pd.Series, q=1 / 3, min_hist=52, below=True) -> pd.Series:
        """True where x sits at or below its q quantile over earlier weeks only, NaN before min_hist."""
        out = pd.Series(np.nan, index=x.index)
        for i in range(len(x)):
            past = x.iloc[:i].dropna()
            if len(past) >= min_hist and not np.isnan(x.iloc[i]):
                cut = past.quantile(q)
                out.iloc[i] = float(x.iloc[i] <= cut) if below else float(x.iloc[i] > cut)
        return out

    def gated(self, w: pd.DataFrame, on: pd.Series) -> pd.DataFrame:
        """The weights where the condition is on, flat elsewhere."""
        return w.where(on.reindex(w.index).fillna(False).astype(bool), 0.0)

    def sleeve(self, w: pd.DataFrame, on: pd.Series, **kw) -> pd.Series:
        """Net returns on the weeks the condition is on, entry and exit cost included."""
        r = self.run(self.gated(w, on), **kw)
        m = on.reindex(r.index).fillna(False).astype(bool)
        # Exit cost lands the week after the gate turns off, so book it to the last on week.
        exits = ~m & m.shift(1, fill_value=False)
        return (r.net + r.cost.where(exits, 0.0).shift(-1).fillna(0.0))[m]


    def condition_walk_forward(self, w: pd.DataFrame, cells: dict, folds=4, history=156, min_on=20):
        """Picks a condition in each fold from earlier weeks only, then trades its gated book.

        cells maps a name to its on mask. At each fold start the cell whose sleeve
        had the best Sharpe before it, among cells on for at least min_on of those
        weeks, is traded through the fold. Returns the stitched gated returns, the
        on mask they traded under, and the cell picked per fold.
        """
        idx = self.run(w).index
        gated = pd.DataFrame({n: self.run(self.gated(w, on)).net.reindex(idx) for n, on in cells.items()}).fillna(0.0)
        on = {n: m.reindex(idx).fillna(False).astype(bool) for n, m in cells.items()}

        def pick(past):
            def score(n):
                live = on[n].reindex(past.index)
                return sharpe(past[n][live]) if live.sum() >= min_on else -np.inf
            return max(on, key=score)

        oos, picked = gate.walk_forward(gated, folds=folds, min_train=history, pick=pick)
        edges = np.linspace(history, len(gated), folds + 1).astype(int)
        mask = pd.concat([on[n].iloc[lo:hi] for n, lo, hi in zip(picked, edges[:-1], edges[1:])])
        return oos, mask, dict(zip([gated.index[lo] for lo in edges[:-1]], picked))


def halves(idx: pd.Index) -> pd.Series:
    """True on the first three of five equal blocks of weeks."""
    train = pd.Series(False, index=idx)
    train.iloc[np.concatenate(np.array_split(np.arange(len(idx)), 5)[:3])] = True
    return train
