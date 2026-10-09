"""Daily bars of every USDT spot pair, read straight from the Binance archive as CSV.

An independent reader to check the qp source against. Each symbol's monthly
daily-kline files are listed and read. A day missing inside a symbol's span is
refilled from the daily files where the archive has them. Bars are keyed on
close time, so a bar is dated the day it was known. Nothing is cached.
"""
from __future__ import annotations

import io
import re
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from concurrent.futures import ThreadPoolExecutor

import pandas as pd

from qp_research.data import universe

ARCHIVE = "https://s3-ap-northeast-1.amazonaws.com/data.binance.vision"
COLUMNS = ['open_time', 'open', 'high', 'low', 'close', 'volume', 'close_time']


def _get(url: str) -> bytes:
    """The body at url, retried on a dropped connection. An HTTP error is raised at once."""
    for attempt in range(4):
        try:
            return urllib.request.urlopen(url, timeout=60).read()
        except urllib.error.HTTPError:
            raise
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            if attempt == 3:
                raise
            time.sleep(2 ** attempt)


def _list(prefix: str, pattern: str) -> list[str]:
    names, marker = [], ''
    while True:
        url = f"{ARCHIVE}?delimiter=/&prefix={urllib.parse.quote(prefix)}" + (f"&marker={urllib.parse.quote(marker)}" if marker else '')
        xml = _get(url).decode()
        names += re.findall(pattern, xml)
        if '<IsTruncated>true</IsTruncated>' not in xml:
            return names
        marker = re.search(r'<NextMarker>([^<]+)</NextMarker>', xml).group(1)


def usdt_pairs() -> list[str]:
    """Every USDT spot pair the archive has carried, delisted included, coins only."""
    prefix = 'data/spot/monthly/klines/'
    names = _list(prefix, rf'<Prefix>{prefix}([^/<]+)/</Prefix>')
    return sorted(n for n in names if n.endswith('USDT') and n[:-4] not in universe.NOT_COINS and not universe.LEVERAGED.search(n))


def _read(key: str) -> pd.DataFrame | None:
    try:
        raw = _get(f"{ARCHIVE}/{urllib.parse.quote(key)}")
    except urllib.error.HTTPError:
        return None
    z = zipfile.ZipFile(io.BytesIO(raw))
    df = pd.read_csv(z.open(z.namelist()[0]), header=None, usecols=range(7), names=COLUMNS)
    # A header row appears in some later files.
    return df[pd.to_numeric(df.open_time, errors='coerce').notna()].astype(float)


def _symbol(sym: str, start: pd.Timestamp, end: pd.Timestamp) -> tuple[pd.DataFrame, list[str]]:
    prefix = f'data/spot/monthly/klines/{sym}/1d/'
    keys = _list(prefix, rf'<Key>({prefix}{sym}-1d-(\d{{4}}-\d{{2}})\.zip)</Key>')
    frames = [_read(k) for k, month in keys if start.replace(day=1) <= pd.Timestamp(month + '-01', tz='UTC') < end]
    frames = [f for f in frames if f is not None and len(f)]
    if not frames:
        return pd.DataFrame(columns=COLUMNS), []
    bars = pd.concat(frames)
    # Stamps are milliseconds, or microseconds in later files.
    unit = bars.close_time.map(lambda t: 'us' if t >= 1e15 else 'ms')
    bars['day'] = [pd.Timestamp(int(t), unit=u, tz='UTC').floor('D') for t, u in zip(bars.close_time, unit)]
    log = []
    span = pd.date_range(bars.day.min(), bars.day.max(), freq='D')
    for day in span.difference(bars.day):
        f = _read(f"data/spot/daily/klines/{sym}/1d/{sym}-1d-{day.date()}.zip")
        if f is not None and len(f):
            f['day'] = day
            bars = pd.concat([bars, f])
            log.append(f'{sym} filled {day.date()} from the daily file')
        else:
            log.append(f'{sym} missing {day.date()}, no daily file')
    bars['symbol'] = sym
    return bars, log


def daily_bars(start: str, end: str, workers=16, log=print) -> pd.DataFrame:
    """Daily high, low, close and volume in long form for days in [start, end)."""
    lo, hi = pd.Timestamp(start, tz='UTC'), pd.Timestamp(end, tz='UTC')
    with ThreadPoolExecutor(workers) as pool:
        parts = list(pool.map(lambda s: _symbol(s, lo, hi), usdt_pairs()))
    for _, lines in parts:
        for line in lines:
            log(line)
    bars = pd.concat([b for b, _ in parts if len(b)], ignore_index=True)
    bars = bars[(bars.day >= lo) & (bars.day < hi)].drop_duplicates(['symbol', 'day'])
    return bars[['symbol', 'day', 'high', 'low', 'close', 'volume']].sort_values(['symbol', 'day']).reset_index(drop=True)
