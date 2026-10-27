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

The bar rules live in long, a lazy polars frame with one row per coin and bar,
which holds at any bar size. bars pivots it into the wide panels a period
panel is built from.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
import pandas as pd
import polars as pl

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


def _lazy(x: pl.LazyFrame | pl.DataFrame | pd.DataFrame) -> pl.LazyFrame:
    return x.lazy() if isinstance(x, pl.DataFrame) else pl.from_pandas(x).lazy() if isinstance(x, pd.DataFrame) else x


def long(rows: pl.LazyFrame | pd.DataFrame, at='day', warmup=30, prints=None, every='1d') -> pl.LazyFrame:
    """Bar-level columns in long form from bars in long form, sorted by symbol then bar.

    Adds traded, dollar_volume, returns and held beside symbol, at, close and
    volume. A return needs the bar before to exist for the coin and to have
    traded, where the bars before are every bar any coin has. With funding
    prints in long form, adds funding per bar, every naming the bar length.
    """
    rows = _lazy(rows)
    prev = lambda c: pl.col(c).shift(1).over('symbol')
    traded = pl.col('volume') > 0
    # numpy's log, so the values match the wide panels bit for bit.
    level = pl.when(traded).then(pl.col('close')).otherwise(np.nan).map_batches(np.log, return_dtype=pl.Float64).fill_nan(None)
    out = (rows.select('symbol', at, 'close', 'volume').sort('symbol', at)
           .with_columns(bar=pl.col(at).rank('dense'), traded=traded, level=level))
    seen = pl.col('traded').cast(pl.Int64).cum_sum().over('symbol')
    valid = pl.col('traded') & prev('traded') & (pl.col('bar') - prev('bar') == 1) & (seen > warmup)
    held = pl.col('level') - pl.col('level').forward_fill().shift(1).over('symbol')
    resumed = pl.any_horizontal([(pl.col('symbol') == sym) & (pl.col(at) == pd.Timestamp(day, tz='UTC'))
                                 for sym, day in universe.REDENOMINATIONS] or [pl.lit(False)])
    out = out.with_columns(
        dollar_volume=pl.when('traded').then(pl.col('volume') * pl.col('close')),
        returns=pl.when(valid).then(pl.col('level') - prev('level')),
        held=pl.when(pl.col('traded') & ~resumed).then(held),
    ).drop('bar', 'level')
    if prints is None:
        return out
    paid = _funding(out.select('symbol', at), _lazy(prints), at, every)
    return out.join(paid, on=['symbol', at], how='left').with_columns(pl.col('funding').fill_null(0.0)).sort('symbol', at)


def _funding(keys: pl.LazyFrame, prints: pl.LazyFrame, at: str, every: str) -> pl.LazyFrame:
    """Funding per coin and bar, summed over the prints each bar pays.

    A print belongs to the bar it falls in. One inside a halt is paid on the bar
    trading resumes, where the halt's gap is booked, since the position is still
    held. Prints before a coin's first bar or after its last are dropped.
    """
    first = keys.group_by('symbol').agg(first=pl.col(at).min())
    # Prints take the bars' time unit, so the two always join.
    stamp = keys.collect_schema()[at]
    p = (prints.select('symbol', pl.col('t').cast(stamp), 'funding_rate').unique(['symbol', 't'], keep='last', maintain_order=True)
         .with_columns(start=pl.col('t').dt.truncate(every)).join(first, on='symbol')
         .filter(pl.col('start') >= pl.col('first')).sort('start'))
    bars = keys.select('symbol', pl.col(at).alias('paid_on')).sort('paid_on')
    # Both sides are sorted on the key above.
    placed = p.join_asof(bars, left_on='start', right_on='paid_on', by='symbol', strategy='forward', check_sortedness=False)
    return (placed.drop_nulls('paid_on').group_by('symbol', 'paid_on').agg(funding=pl.col('funding_rate').sum())
            .rename({'paid_on': at}))


def rolling(bars: pl.LazyFrame, *, at: str, vol: str, vol_min: int, liquidity: str, liquidity_min: int) -> pl.LazyFrame:
    """Adds vol and liquidity to bars in long form, both as of the coin's bar before, so they are known at trade time.

    vol is the spread of signal returns and liquidity the median dollar volume,
    each over a trailing span of time such as '720h' that ends on the bar, with
    at least the given count of values in it. Spans are in time, so a missing
    bar shortens a window rather than stretching it.
    """
    window = lambda span, least: {'by': at, 'window_size': span, 'min_samples': least}
    by = {'partition_by': 'symbol'}
    return bars.with_columns(
        vol=pl.col('returns').rolling_std_by(**window(vol, vol_min)).over(**by).shift(1).over(**by),
        liquidity=pl.col('dollar_volume').rolling_median_by(**window(liquidity, liquidity_min)).over(**by).shift(1).over(**by))


def market(bars: pl.LazyFrame, *, at: str, floor: float, vol: str, vol_min: int) -> pl.LazyFrame:
    """Adds the market's return per bar, each coin's residual beyond it, and the residual's vol as of the coin's bar before.

    The market is the average signal return of coins with liquidity of at least
    floor on the bar. residual_vol is the spread of residuals over a trailing
    span of time, with at least vol_min values in it.
    """
    m = (bars.filter((pl.col('liquidity') >= floor) & pl.col('returns').is_not_null())
         .group_by(at).agg(market=pl.col('returns').mean()))
    by = {'partition_by': 'symbol'}
    return (bars.join(m, on=at, how='left').sort('symbol', at)
            .with_columns(residual=pl.col('returns') - pl.col('market'))
            .with_columns(residual_vol=pl.col('residual').rolling_std_by(by=at, window_size=vol, min_samples=vol_min).over(**by).shift(1).over(**by)))


def bars(rows: pd.DataFrame | pl.LazyFrame, prints: pd.DataFrame | None = None, at='day', warmup=30) -> Bars:
    """Bar-level panels from bars in long form, and funding prints in long form when given.

    at names the bar time column. Prints are placed on the bar they fall in.
    """
    lf = long(rows, at, warmup).collect()
    if lf.select(pl.struct('symbol', at).is_duplicated().any()).item():
        raise ValueError(f'more than one bar per symbol and {at}')
    symbols = pd.Index(sorted(lf['symbol'].unique().to_list()), name='symbol')
    index = pd.DatetimeIndex(lf[at].unique().sort().to_pandas(), name=at)
    row = index.get_indexer(lf[at].to_pandas())
    col = symbols.get_indexer(lf['symbol'].to_numpy())

    def wide(c):
        out = np.full((len(index), len(symbols)), np.nan)
        out[row, col] = lf[c].to_numpy()
        return pd.DataFrame(out, index=index, columns=symbols)

    close, volume = wide('close'), wide('volume')
    traded = volume > 0
    funding = pd.DataFrame(0.0, index=close.index, columns=close.columns)
    if prints is not None and len(prints):
        step = pd.Series(close.index).diff().median()
        prints = prints.drop_duplicates(['symbol', 't'], keep='last')
        per_bar = prints.assign(bar=prints.t.dt.floor(step)).pivot_table(index='bar', columns='symbol', values='funding_rate', aggfunc='sum')
        funding = per_bar.reindex(index=close.index, columns=close.columns).fillna(0.0)
    return Bars(close=close, traded=traded, dollar_volume=wide('dollar_volume'), returns=wide('returns'),
                held=wide('held'), funding=funding)


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
