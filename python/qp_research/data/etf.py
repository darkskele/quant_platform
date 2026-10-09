"""Daily bars for a fixed list of US-listed ETFs across asset classes, from Yahoo.

Close is the adjusted close, so a return carries its dividends. Adjustment is
rewritten whenever a fund pays, so one fetch covers a whole window and is
cached as one partition. Volume is in shares. The list is every fund still
listed today, so funds that closed are missing.
"""
from __future__ import annotations

import json
import time
import urllib.request

import pandas as pd

from .. import cache as cache_

UNIVERSE = {
    'us equity': ['SPY', 'QQQ', 'IWM', 'DIA', 'MDY', 'IWD', 'IWF'],
    'us sectors': ['XLB', 'XLE', 'XLF', 'XLI', 'XLK', 'XLP', 'XLU', 'XLV', 'XLY', 'IYR', 'VNQ'],
    'world equity': ['EFA', 'EEM', 'EWJ', 'EWG', 'EWU', 'EWC', 'EWA', 'EWZ', 'EWT', 'EWY', 'EWH', 'EWW', 'FXI', 'EZA',
                     'EWS', 'EWL', 'EWP', 'EWI', 'EWQ', 'INDA', 'RWX'],
    'bonds': ['TLT', 'IEF', 'SHY', 'LQD', 'HYG', 'TIP', 'AGG', 'EMB', 'MUB', 'BWX'],
    'commodities': ['GLD', 'SLV', 'USO', 'UNG', 'DBC', 'DBA', 'DBB', 'PPLT', 'CPER', 'GDX'],
    'currencies': ['UUP', 'FXE', 'FXY', 'FXB', 'FXA', 'FXC', 'FXF'],
}


def symbols() -> list[str]:
    return [s for group in UNIVERSE.values() for s in group]


def _fetch(sym: str, start: pd.Timestamp, end: pd.Timestamp) -> pd.DataFrame:
    url = (f'https://query1.finance.yahoo.com/v8/finance/chart/{sym}?period1={int(start.timestamp())}'
           f'&period2={int(end.timestamp())}&interval=1d&events=div%2Csplits')
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    r = json.loads(urllib.request.urlopen(req, timeout=30).read())['chart']['result'][0]
    out = pd.DataFrame({'symbol': sym, 'day': pd.to_datetime(r['timestamp'], unit='s', utc=True).floor('D'),
                        'close': r['indicators']['adjclose'][0]['adjclose'], 'volume': r['indicators']['quote'][0]['volume']})
    out = out.dropna(subset=['close']).drop_duplicates('day')
    out['volume'] = out.volume.fillna(0.0)
    return out


def daily_bars(start, end, syms=None, cache=None) -> pd.DataFrame:
    """Adjusted close and share volume in long form for days in [start, end), cached."""
    lo, hi = cache_.utc(start), cache_.utc(end)

    def fetch(names, a, b):
        frames = []
        for s in names:
            frames.append(_fetch(s, a, b))
            time.sleep(0.3)
        return pd.concat(frames, ignore_index=True), b

    rows = (cache or cache_.default()).load('yahoo', 'etf', 'daily', syms or symbols(), cache_.span(lo, hi), hi, fetch, 'day')
    return rows.sort_values(['symbol', 'day']).reset_index(drop=True)
