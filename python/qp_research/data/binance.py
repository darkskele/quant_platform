"""Binance archive datasets streamed through the qp source and cached by partition.

Closed months come from monthly files and the rest from daily files, as far as
the archive has published. A dataset published at one cadence only uses that
one. Kline days missing from a monthly file inside a symbol's span are refilled
from the daily files. Every frame carries the event time t, a kline also its
open_time.
"""
from __future__ import annotations

import logging
from dataclasses import dataclass

import pandas as pd

from .. import cache as cache_, engine

log = logging.getLogger(__name__)

DAILY_LAG = pd.Timedelta(days=1)     # a day's file is assumed published once this long has passed since it closed
MONTHLY_LAG = pd.Timedelta(days=7)   # likewise a month's file
MARKETS = {'usdm': 'UsdM', 'spot': 'Spot', 'coinm': 'CoinM'}
SLOW = {'1d', '3d', '1w', '1M'}      # kline intervals partitioned by year rather than month


@dataclass(frozen=True)
class Dataset:
    """One archive dataset, the event it parses into, and the payload fields kept."""
    endpoint: str
    event: str
    fields: tuple[str, ...]
    cadence: str    # 'both', 'monthly' or 'daily', as the archive publishes it


KLINE = ('open_time', 'open', 'high', 'low', 'close', 'volume')
PRICE = ('open_time', 'open', 'high', 'low', 'close')
DATASETS = {
    'klines': Dataset('Klines', 'Kline', KLINE, 'both'),
    'mark_klines': Dataset('MarkPriceKlines', 'MarkPriceKline', PRICE, 'both'),
    'premium_klines': Dataset('PremiumIndexKlines', 'PremiumIndexKline', PRICE, 'both'),
    'funding': Dataset('FundingRate', 'Funding', ('funding_rate', 'interval_hours'), 'monthly'),
    'metrics': Dataset('Metrics', 'OpenInterest', ('open_interest', 'open_interest_value', 'toptrader_account_ratio',
                                                   'toptrader_position_ratio', 'account_long_short_ratio',
                                                   'taker_long_short_volume_ratio'), 'daily'),
}


def horizon(now: pd.Timestamp) -> tuple[pd.Timestamp, pd.Timestamp]:
    """Ends of the published daily files and the published monthly files, as of now."""
    daily = cache_.utc(now).floor('D') - DAILY_LAG
    return daily, (daily - MONTHLY_LAG).replace(day=1)


def plan(cadence: str, start: pd.Timestamp, end: pd.Timestamp, now: pd.Timestamp):
    """Fetch windows as (cadence, from, to) over [start, end), and the time the result is complete through."""
    daily_end, monthly_end = horizon(now)
    if cadence == 'daily':
        hi = min(end, daily_end)
        return ([('Daily', start, hi)] if start < hi else []), max(start, hi)
    m_hi = min(end, monthly_end)
    windows = [('Monthly', start, m_hi)] if start < m_hi else []
    if cadence == 'monthly':
        return windows, max(start, m_hi)
    d_lo, d_hi = max(start, m_hi), min(end, daily_end)
    if d_lo < d_hi:
        windows.append(('Daily', d_lo, d_hi))
    return windows, max(start, d_hi)


def runs(days: pd.DatetimeIndex) -> list[tuple[pd.Timestamp, pd.Timestamp]]:
    """Contiguous runs of a sorted day index, as (first, last) pairs."""
    out = []
    for d in days:
        if out and d - out[-1][1] == pd.Timedelta(days=1):
            out[-1][1] = d
        else:
            out.append([d, d])
    return [tuple(r) for r in out]


def _stream(market: str, ds: Dataset, interval: str, symbols: list[str], cadence: str, lo, hi) -> pd.DataFrame:
    """Every event of one dataset for symbols from files starting in [lo, hi)."""
    q = engine.module()
    slot = int(getattr(q.BinanceMarket, MARKETS[market]))
    builder = q.SubscriptionBuilder()
    for s in symbols:
        builder.add(q.ExchangeId.Binance, slot, s)
    sub = builder.build()
    name = {sub.resolve(q.ExchangeId.Binance, slot, s).symbol: s for s in symbols}

    pool = q.FetchPoolConfig()
    pool.workers = 32
    spec = q.StreamSpec(getattr(q.EndpointKind, ds.endpoint), interval or '')
    cfg = q.BinanceHistoricalConfig([spec], getattr(q.Cadence, cadence), int(lo.value), int(hi.value) - 1, pool)
    src = q.BinanceHistoricalSource(sub, cfg)
    src.plan()
    fields = ds.fields
    rows = [(name[e.base.symbol], e.base.ts, *(getattr(e.payload, f) for f in fields)) for e in src]
    failed = src.fetch_stats().completed_failed
    if failed:
        detail = '; '.join(f'{f.url} {f.detail}' for f in src.fetch_failures()[:5])
        raise RuntimeError(f'{failed} archive fetches failed, nothing cached. {detail}')
    out = pd.DataFrame(rows, columns=['symbol', 't', *fields])
    out['t'] = pd.to_datetime(out.t, unit='ns', utc=True)
    if 'open_time' in out:
        out['open_time'] = pd.to_datetime(out.open_time, unit='ns', utc=True)
    return out


def _refill(market, ds, interval, rows: pd.DataFrame, before: pd.Timestamp) -> pd.DataFrame:
    """Kline days missing inside each symbol's span before the given time, refetched from daily files.

    Symbols missing the same run of days share one fetch.
    """
    gaps = {}
    for sym, g in rows.groupby('symbol'):
        days = g.open_time.dt.floor('D')
        missing = pd.date_range(days.min(), days.max(), freq='D').difference(days)
        for run in runs(missing[missing < before]):
            gaps.setdefault(run, []).append(sym)
    fills = []
    for (first, last), syms in gaps.items():
        got = _stream(market, ds, interval, syms, 'Daily', first, last + pd.Timedelta(days=1))
        log.info('%d symbols %s filled %s to %s from daily files, %d rows', len(syms), interval, first.date(), last.date(), len(got))
        fills.append(got)
    if not fills:
        return rows
    return pd.concat([rows, *fills], ignore_index=True).drop_duplicates(['symbol', 'open_time'])


def load(dataset: str, symbols, start, end, interval=None, market='usdm', cache=None, now=None) -> pd.DataFrame:
    """Rows of one dataset for symbols with stamps in [start, end), long form, cached by partition."""
    ds = DATASETS[dataset]
    now = pd.Timestamp.now(tz='UTC') if now is None else cache_.utc(now)
    lo, hi = cache_.utc(start), cache_.utc(end)
    stamp = 'open_time' if 'open_time' in ds.fields else 't'
    yearly = dataset == 'funding' or (interval in SLOW and stamp == 'open_time')
    parts = (cache_.years if yearly else cache_.months)(lo, hi)
    name = f'{dataset}_{interval}' if interval else dataset

    def fetch(syms, a, b):
        windows, through = plan(ds.cadence, a, b, now)
        frames = [_stream(market, ds, interval, syms, cad, x, y) for cad, x, y in windows]
        rows = pd.concat(frames, ignore_index=True) if frames else pd.DataFrame(columns=['symbol', 't', *ds.fields])
        monthly = [y for cad, _, y in windows if cad == 'Monthly']
        if stamp == 'open_time' and monthly and len(rows):
            rows = _refill(market, ds, interval, rows, monthly[-1])
        return rows, through

    _, need = plan(ds.cadence, parts[0].start, hi, now)
    rows = (cache or cache_.default()).load('binance', market, name, list(symbols), parts, need, fetch, stamp)
    if not len(rows):
        return pd.DataFrame(columns=['symbol', 't', *ds.fields])
    rows = rows[(rows[stamp] >= lo) & (rows[stamp] < hi)]
    return rows.sort_values(['symbol', stamp]).reset_index(drop=True)


def klines(symbols, start, end, interval='1d', market='usdm', **kw) -> pd.DataFrame:
    """Klines with open_time in [start, end)."""
    return load('klines', symbols, start, end, interval, market, **kw)


def funding(symbols, start, end, market='usdm', **kw) -> pd.DataFrame:
    """Funding prints stamped in [start, end). Positive means longs pay."""
    return load('funding', symbols, start, end, None, market, **kw)


def metrics(symbols, start, end, market='usdm', **kw) -> pd.DataFrame:
    """Open interest and positioning ratios per 5-minute sample stamped in [start, end)."""
    return load('metrics', symbols, start, end, None, market, **kw)


def daily_bars(symbols, start, end, market='usdm', **kw) -> pd.DataFrame:
    """Daily high, low, close and volume, dated the day each bar covers, for days in [start, end)."""
    k = klines(symbols, start, end, '1d', market, **kw)
    k['day'] = k.open_time.dt.floor('D')
    return k[['symbol', 'day', 'high', 'low', 'close', 'volume']]
