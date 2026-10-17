"""Event books turn triggers on long-form bars into trades and a daily net return series.

A trigger is known at its bar's close. Its trade fills at the close delay bars
later and earns from the bar after. It exits at the first close where its
return reaches the profit target or the stop, both in units of the coin's vol
at entry, or at the time stop. Barriers are read on closes only, so a stop that
gaps fills at the close, never at the barrier. A halted coin exits on the bar
trading resumes, and a coin that ends exits on its last bar.

Every candidate trade is found first, then a walk in time order applies the
overlap rules, at most per_coin open trades in a coin and slots across the
book, stronger triggers first at the same close. Each trade holds an equal
slot of capital, so the book is never more than fully invested.

Cost is charged one way at entry and exit through the book's cost model, on
liquidity scaled to a day. Funding is paid on every bar held. The daily series
marks open trades to market, so a trade counts every day it is held. The
effective count of independent trades is the sum of each trade's average
uniqueness, one over the trades open beside it.
"""
from __future__ import annotations

import heapq
from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import pandas as pd
import polars as pl

from .costs import CostModel


@dataclass(frozen=True)
class Exits:
    """Profit target and stop in units of entry vol, None for none, and the time stop in bars."""
    take: float | None
    stop: float | None
    hold: int
    delay: int = 0


@dataclass
class Result:
    trades: pd.DataFrame   # one row per trade taken
    daily: pd.DataFrame    # gross, funding, cost and net per day, open trades marked to market
    effective: float       # independent trades, the sum of each trade's average uniqueness


class EventBook:
    """Triggers in, trades and daily net returns out, on long-form bars priced by one cost model.

    bars carries symbol, at, traded, held, funding, vol and liquidity, as from
    panel.long with prints and panel.rolling. day_scale turns its liquidity into
    the daily dollar volume the cost model is fitted on, 24 for hourly bars.
    """

    def __init__(self, bars: pl.LazyFrame | pl.DataFrame, cost: CostModel, *, at='open_time', day_scale: float,
                 slots=10, per_coin=1, ledger=None):
        self.at, self.cost, self.day_scale = at, cost, day_scale
        self.slots, self.per_coin, self.ledger = slots, per_coin, ledger
        b = bars.lazy().select('symbol', at, 'traded', 'held', 'funding', 'vol', 'liquidity').sort('symbol', at)
        # Each coin's bars numbered, and its held log return summed, so any span's return is a difference.
        self.bars = b.with_columns(i=pl.int_range(pl.len()).over('symbol'),
                                   level=pl.col('held').fill_null(0.0).cum_sum().over('symbol')).collect()

    def candidates(self, triggers: pl.DataFrame | pd.DataFrame, exits: Exits) -> pl.DataFrame:
        """Every trade the triggers would open, with its exit, before any overlap rule.

        triggers carries symbol, at, side, +1 long or -1 short, and strength,
        which orders triggers at the same close.
        """
        t = triggers if isinstance(triggers, pl.DataFrame) else pl.from_pandas(triggers)
        b, at = self.bars, self.at
        t = t.with_columns(pl.col(at).cast(b.schema[at]))
        on = t.select('symbol', at, 'side', 'strength').join(b.select('symbol', at, 'i', 'vol'), on=['symbol', at])
        entry = (on.with_columns(e=pl.col('i') + exits.delay).drop(at, 'i')
                 .join(b.select('symbol', pl.col('i').alias('e'), pl.col(at).alias('entry'), 'traded',
                                pl.col('level').alias('entry_level')), on=['symbol', 'e'])
                 .filter(pl.col('traded') & (pl.col('vol') > 0)).drop('traded'))
        entry = entry.with_row_index('trade')
        # Each trade's path over the hold, read on traded closes only.
        path = (entry.select('trade', 'symbol', 'e', 'side', 'vol', 'entry_level')
                .with_columns(k=pl.int_ranges(1, exits.hold + 1)).explode('k').with_columns(i=pl.col('e') + pl.col('k'))
                .join(b.select('symbol', 'i', 'traded', 'level'), on=['symbol', 'i'])
                .with_columns(move=pl.col('side') * (pl.col('level') - pl.col('entry_level')) / pl.col('vol')))
        hit = []
        if exits.take is not None:
            hit.append((pl.col('move') >= exits.take).alias('take'))
        if exits.stop is not None:
            hit.append((pl.col('move') <= -exits.stop).alias('stop'))
        first = (path.filter(pl.col('traded') & pl.any_horizontal(hit or [pl.lit(False)]))
                 .group_by('trade').agg(pl.col('i').min().alias('x_hit'),
                                        pl.col('move').sort_by('i').first().alias('hit_move')))
        last = b.group_by('symbol').agg(last=pl.col('i').max())
        out = (entry.join(first, on='trade', how='left').join(last, on='symbol')
               .with_columns(due=pl.min_horizontal(pl.col('e') + exits.hold, pl.col('last'))))
        # A time stop on a halted bar waits for the bar trading resumes.
        traded = b.filter('traded').select('symbol', pl.col('i').alias('due'), pl.col('i').alias('x_time')).sort('due')
        out = (out.sort('due').join_asof(traded, on='due', by='symbol', strategy='forward', check_sortedness=False)
               .with_columns(x_time=pl.coalesce('x_time', 'last')))
        reason = (pl.when(pl.col('x_hit').is_not_null())
                  .then(pl.when(pl.col('hit_move') > 0).then(pl.lit('take')).otherwise(pl.lit('stop')))
                  .when(pl.col('x_time') == pl.col('e') + exits.hold).then(pl.lit('time'))
                  .when(pl.col('x_time') > pl.col('e') + exits.hold).then(pl.lit('halt'))
                  .otherwise(pl.lit('end')))
        out = out.with_columns(x=pl.coalesce('x_hit', 'x_time'), reason=reason)
        out = out.join(b.select('symbol', pl.col('i').alias('x'), pl.col(at).alias('exit')), on=['symbol', 'x'])
        return out.select('trade', 'symbol', 'side', 'strength', 'vol', 'e', 'entry', 'x', 'exit', 'reason').sort('trade')

    def walk(self, cands: pl.DataFrame) -> pl.DataFrame:
        """The candidates taken under the overlap rules, in time order, stronger first at the same close."""
        order = cands.with_columns(neg=-pl.col('strength').abs()).sort('entry', 'neg', 'trade')
        open_all, by_coin, taken = [], defaultdict(list), []
        for trade, sym, entry, exit_ in order.select('trade', 'symbol', 'entry', 'exit').iter_rows():
            while open_all and open_all[0] <= entry:
                heapq.heappop(open_all)
            held = by_coin[sym] = [x for x in by_coin[sym] if x > entry]
            if len(held) >= self.per_coin or len(open_all) >= self.slots:
                continue
            heapq.heappush(open_all, exit_)
            held.append(exit_)
            taken.append(trade)
        return cands.filter(pl.col('trade').is_in(taken))

    def run(self, triggers, exits: Exits, name=None, kind='exploratory', params=None) -> Result:
        """Trades taken, the daily book and its effective trade count. With a ledger the daily net is one trial."""
        trades = self.walk(self.candidates(triggers, exits))
        b, at, n = self.bars, self.at, 1.0 / self.slots
        # Every bar a trade is held, from the bar after entry to its exit.
        rows = (trades.select('trade', 'symbol', 'side', 'e', 'x').with_columns(i=pl.int_ranges(pl.col('e') + 1, pl.col('x') + 1))
                .explode('i').join(b.select('symbol', 'i', at, 'level', 'funding'), on=['symbol', 'i'])
                .join(b.select('symbol', pl.col('i').alias('e'), pl.col('level').alias('entry_level')), on=['symbol', 'e'])
                .sort('trade', 'i'))
        # Simple return on the entry notional, so a position is held in units, not rebalanced.
        value = (pl.col('level') - pl.col('entry_level')).exp()
        before = value.shift(1, fill_value=1.0).over('trade')
        rows = rows.with_columns(gross=n * pl.col('side') * (value - before),
                                 funding=-n * pl.col('side') * before * pl.col('funding'))
        cost = self._cost(trades, b, n)
        per_day = lambda df, cols: df.group_by(pl.col(at).dt.truncate('1d').alias('day')).agg(*[pl.col(c).sum() for c in cols])
        daily = (per_day(rows, ['gross', 'funding']).join(per_day(cost, ['cost']), on='day', how='full', coalesce=True)
                 .fill_null(0.0).sort('day').to_pandas().set_index('day'))
        if len(daily):
            daily = daily.reindex(pd.date_range(daily.index[0], daily.index[-1], freq='D', name='day'), fill_value=0.0)
        daily['net'] = daily.gross + daily.funding + daily.cost
        # Uniqueness, one over the trades open on each bar a trade is held, averaged over the trade.
        open_on = rows.group_by(at).agg(open=pl.len())
        unique = rows.join(open_on, on=at).group_by('trade').agg((1 / pl.col('open')).mean().alias('u'))
        effective = float(unique['u'].sum()) if len(unique) else 0.0
        book = (rows.group_by('trade').agg(pl.col('gross').sum(), pl.col('funding').sum())
                .join(cost.group_by('trade').agg(pl.col('cost').sum()), on='trade', how='left'))
        out = (trades.join(book, on='trade', how='left').fill_null(0.0)
               .with_columns(bars=pl.col('x') - pl.col('e'), net=pl.col('gross') + pl.col('funding') + pl.col('cost'))
               .drop('e', 'x').sort('entry', 'trade').to_pandas())
        if self.ledger is not None:
            self.ledger.record(daily.net, name, kind, params)
        return Result(trades=out, daily=daily, effective=effective)

    def _cost(self, trades: pl.DataFrame, b: pl.DataFrame, n: float) -> pl.DataFrame:
        """One-way cost at entry on the slot and at exit on the position's value then, each on its own bar."""
        legs = []
        for col, leg in (('e', 'entry'), ('x', 'exit')):
            legs.append(trades.select('trade', 'symbol', 'e', pl.col(col).alias('i'))
                        .join(b.select('symbol', 'i', self.at, 'liquidity', 'level'), on=['symbol', 'i'])
                        .join(b.select('symbol', pl.col('i').alias('e'), pl.col('level').alias('entry_level')), on=['symbol', 'e'])
                        .with_columns(dw=n * ((pl.col('level') - pl.col('entry_level')).exp() if leg == 'exit' else pl.lit(1.0))))
        legs = pl.concat(legs)
        liq, dw = legs['liquidity'].to_numpy() * self.day_scale, legs['dw'].to_numpy()
        bps = np.asarray(self.cost.bps(liq, dw), dtype=float)
        return legs.select('trade', self.at).with_columns(cost=pl.Series(-dw * bps / 1e4))
