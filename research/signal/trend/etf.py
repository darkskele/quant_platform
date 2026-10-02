"""Daily bars for a fixed list of US-listed ETFs across asset classes, from Yahoo.

Close is the adjusted close, so a return carries its dividends. Volume is in
shares, so dollar volume on the adjusted close runs slightly under the true
figure in early years. The list is every fund still listed today, so funds
that closed are missing.
"""
from __future__ import annotations

import json
import time
import urllib.request

import pandas as pd

import stream

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


def _fetch(sym: str, start: str, end: str) -> pd.DataFrame:
    p1, p2 = (int(pd.Timestamp(t, tz='UTC').timestamp()) for t in (start, end))
    url = f'https://query1.finance.yahoo.com/v8/finance/chart/{sym}?period1={p1}&period2={p2}&interval=1d&events=div%2Csplits'
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    r = json.loads(urllib.request.urlopen(req, timeout=30).read())['chart']['result'][0]
    day = pd.to_datetime(r['timestamp'], unit='s', utc=True).floor('D')
    out = pd.DataFrame({'symbol': sym, 'day': day, 'close': r['indicators']['adjclose'][0]['adjclose'],
                        'volume': r['indicators']['quote'][0]['volume']})
    return out.dropna(subset=['close']).drop_duplicates('day')


def daily_bars(start: str, end: str, log=print) -> pd.DataFrame:
    """Adjusted close and share volume in long form for days in [start, end), cached."""
    path = stream.CACHE / f'etf_daily_{pd.Timestamp(start).date()}_{pd.Timestamp(end).date()}.parquet'
    if path.exists():
        return pd.read_parquet(path)
    bars = []
    for sym in symbols():
        bars.append(_fetch(sym, start, end))
        log(f'{sym} {len(bars[-1])} days from {bars[-1].day.min().date()}')
        time.sleep(0.3)
    out = pd.concat(bars, ignore_index=True)
    out['volume'] = out.volume.fillna(0.0)
    out.to_parquet(path)
    return out
