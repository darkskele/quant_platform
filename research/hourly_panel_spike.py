"""Spike: the full-universe hourly panel through qp_research, lazily, end to end.

data.scan reads the hourly klines from the cache, panel.long applies the bar
rules the weekly panels use and panel.rolling adds the prior 720h vol and median
dollar volume. On top, the equal-weight market return over liquid coins and the
|z| > 4 triggers. Prints wall time and peak RSS. --full also collects the whole
panel, to size the buffer a notebook needs.
"""
from __future__ import annotations

import argparse
import resource
import time

import polars as pl

from qp_research import data, panel
from qp_research.data import universe


def hourly() -> pl.LazyFrame:
    rows = data.scan('klines', universe.perps(), '2020-01-01', '2026-09-01', '1h')
    b = panel.long(rows, at='open_time', warmup=0)
    b = panel.rolling(b, at='open_time', vol='720h', vol_min=360, liquidity='720h', liquidity_min=360)
    return b.with_columns(z=pl.col('returns') / pl.col('vol'), liquid=pl.col('liquidity') > 1e6)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--full', action='store_true')
    args = ap.parse_args()

    t0 = time.perf_counter()
    p = hourly()
    market, triggers = pl.collect_all([
        p.filter('liquid').group_by('open_time').agg(ret=pl.col('returns').mean(), coins=pl.len()).sort('open_time'),
        p.filter(pl.col('liquid') & (pl.col('z').abs() > 4)).select('symbol', 'open_time', 'returns', 'z'),
    ])
    print(f'market {market.shape}, triggers {triggers.shape}, {time.perf_counter() - t0:.1f}s')
    if args.full:
        t1 = time.perf_counter()
        full = p.collect()
        print(f'full panel {full.shape}, {full.estimated_size() / 1e9:.2f} GB, {time.perf_counter() - t1:.1f}s')
    print(f'peak RSS {resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1e6:.2f} GB')


if __name__ == '__main__':
    main()
