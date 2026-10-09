"""A local cache of fetched data, one parquet file per partition.

A partition is one dataset, one symbol and one span, usually a year or a month.
The index records how far each partition is complete, so a request reads what
it has and fetches only what is missing or not complete far enough. Past a
size cap the least recently read partitions are evicted.

Root is QP_CACHE_DIR, else ~/.cache/qp-research/store. The cap is
QP_CACHE_MAX_GB, else 50.
"""
from __future__ import annotations

import os
import sqlite3
import time
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import pandas as pd
import pyarrow.dataset as pds

NS = 1_000_000_000

SCHEMA = """
CREATE TABLE IF NOT EXISTS partitions (
    venue TEXT NOT NULL, market TEXT NOT NULL, dataset TEXT NOT NULL, symbol TEXT NOT NULL,
    start INTEGER NOT NULL, end INTEGER NOT NULL, through INTEGER NOT NULL,
    rows INTEGER NOT NULL, bytes INTEGER NOT NULL, path TEXT,
    fetched_at INTEGER NOT NULL, last_read INTEGER NOT NULL,
    PRIMARY KEY (venue, market, dataset, symbol, start))
"""


def utc(t) -> pd.Timestamp:
    """t as a UTC timestamp, naive input read as UTC."""
    t = pd.Timestamp(t)
    return t.tz_localize('UTC') if t.tzinfo is None else t.tz_convert('UTC')


@dataclass(frozen=True)
class Part:
    """One partition's span, [start, end), and its folder label."""
    start: pd.Timestamp
    end: pd.Timestamp
    label: str


def years(start, end) -> list[Part]:
    """Calendar-year partitions covering [start, end)."""
    first, last = utc(start), utc(end)
    return [Part(pd.Timestamp(y, 1, 1, tz='UTC'), pd.Timestamp(y + 1, 1, 1, tz='UTC'), f'year={y}')
            for y in range(first.year, (last - pd.Timedelta(1)).year + 1)]


def months(start, end) -> list[Part]:
    """Calendar-month partitions covering [start, end)."""
    first = utc(start).replace(day=1).normalize()
    out = []
    while first < utc(end):
        nxt = first + pd.offsets.MonthBegin(1)
        out.append(Part(first, nxt, f'year={first.year}/month={first.month:02d}'))
        first = nxt
    return out


def span(start, end) -> list[Part]:
    """One partition for exactly [start, end)."""
    lo, hi = utc(start), utc(end)
    return [Part(lo, hi, f'span={lo.date()}_{hi.date()}')]


Fetch = Callable[[list[str], pd.Timestamp, pd.Timestamp], tuple[pd.DataFrame, pd.Timestamp]]


class Cache:
    """Partitions on disk under root, indexed in one sqlite file beside them."""

    def __init__(self, root=None, max_gb=None):
        self.root = Path(root or os.environ.get('QP_CACHE_DIR', Path.home() / '.cache' / 'qp-research' / 'store'))
        self.max_bytes = int(float(max_gb or os.environ.get('QP_CACHE_MAX_GB', 50)) * 1e9)
        self.root.mkdir(parents=True, exist_ok=True)
        with self._db() as db:
            db.execute(SCHEMA)

    def _db(self) -> sqlite3.Connection:
        return sqlite3.connect(self.root / 'index.sqlite', timeout=60)

    def load(self, venue: str, market: str, dataset: str, symbols: list[str], parts: list[Part],
             need: pd.Timestamp, fetch: Fetch, stamp: str, chunk=50) -> pd.DataFrame:
        """Rows for symbols over parts, fetching what is missing or not complete as far as need.

        fetch(symbols, start, end) returns rows with a symbol column and the
        time its rows are complete through. stamp names the column rows are
        partitioned on.
        """
        key = (venue, market, dataset)
        have = self._index(key, symbols)
        stale = defaultdict(list)
        for sym in symbols:
            gaps = tuple(p for p in parts if have.get((sym, _ns(p.start)), -1) < _ns(min(p.end, need)))
            if gaps:
                stale[gaps].append(sym)
        for gaps, syms in stale.items():
            for i in range(0, len(syms), chunk):
                part_syms = syms[i:i + chunk]
                rows, through = fetch(part_syms, gaps[0].start, gaps[-1].end)
                self._write(key, part_syms, gaps, rows, through, stamp)
        out, read = self._read(key, symbols, parts)
        self.trim(keep=read)
        return out

    def _index(self, key, symbols) -> dict:
        wanted = set(symbols)
        with self._db() as db:
            q = 'SELECT symbol, start, through FROM partitions WHERE venue=? AND market=? AND dataset=?'
            return {(s, st): th for s, st, th in db.execute(q, key) if s in wanted}

    def _write(self, key, symbols, parts, rows: pd.DataFrame, through: pd.Timestamp, stamp: str):
        now = time.time_ns()
        entries = []
        by_symbol = dict(tuple(rows.groupby('symbol'))) if len(rows) else {}
        for sym in symbols:
            got = by_symbol.get(sym)
            for p in parts:
                part = got[(got[stamp] >= p.start) & (got[stamp] < p.end)] if got is not None else None
                path, size, n = None, 0, 0
                if part is not None and len(part):
                    rel = Path(*key) / f'symbol={sym}' / p.label / 'part.parquet'
                    dest = self.root / rel
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    tmp = dest.with_suffix(f'.{os.getpid()}.tmp')
                    part.drop(columns='symbol').reset_index(drop=True).to_parquet(tmp, compression='zstd')
                    os.replace(tmp, dest)
                    path, size, n = str(rel), dest.stat().st_size, len(part)
                entries.append((*key, sym, _ns(p.start), _ns(p.end), _ns(min(p.end, through)), n, size, path, now, now))
        with self._db() as db:
            db.executemany('INSERT OR REPLACE INTO partitions VALUES (?,?,?,?,?,?,?,?,?,?,?,?)', entries)

    def _read(self, key, symbols, parts) -> tuple[pd.DataFrame, set[str]]:
        """The rows of every requested partition on disk, and their paths."""
        starts, wanted = {_ns(p.start) for p in parts}, set(symbols)
        with self._db() as db:
            q = 'SELECT symbol, start, path FROM partitions WHERE venue=? AND market=? AND dataset=? AND path IS NOT NULL'
            found = [(s, path) for s, st, path in db.execute(q, key) if st in starts and s in wanted]
            stamp = time.time_ns()
            db.executemany('UPDATE partitions SET last_read=? WHERE path=?', [(stamp, p) for _, p in found])

        read = {p for _, p in found}
        if not found:
            return pd.DataFrame(columns=['symbol']), read
        # Symbol and span come back from the folder names, only symbol is kept.
        table = pds.dataset([str(self.root / p) for _, p in found], format='parquet', partition_base_dir=str(self.root.joinpath(*key)),
                            partitioning=pds.partitioning(flavor='hive')).to_table()
        out = table.drop_columns([c for c in ('year', 'month', 'span') if c in table.column_names]).to_pandas()
        out['symbol'] = out.symbol.astype(str)
        return out[['symbol', *[c for c in out.columns if c != 'symbol']]], read

    def info(self) -> pd.DataFrame:
        """Partitions, rows, gigabytes and the last read per dataset."""
        with self._db() as db:
            q = '''SELECT venue, market, dataset, COUNT(*), SUM(rows), SUM(bytes) / 1e9, MAX(last_read)
                   FROM partitions GROUP BY venue, market, dataset'''
            out = pd.DataFrame(db.execute(q).fetchall(),
                               columns=['venue', 'market', 'dataset', 'partitions', 'rows', 'gb', 'last read'])
        out['last read'] = pd.to_datetime(out['last read'], unit='ns', utc=True)
        return out

    def trim(self, max_gb=None, keep=frozenset()) -> int:
        """Evicts least recently read partitions until the cache fits, sparing keep. Returns how many went."""
        cap = self.max_bytes if max_gb is None else int(max_gb * 1e9)
        with self._db() as db:
            total = db.execute('SELECT COALESCE(SUM(bytes), 0) FROM partitions').fetchone()[0]
            if total <= cap:
                return 0
            gone = []
            for path, size in db.execute('SELECT path, bytes FROM partitions WHERE path IS NOT NULL ORDER BY last_read'):
                if total <= cap:
                    break
                if path in keep:
                    continue
                (self.root / path).unlink(missing_ok=True)
                gone.append(path)
                total -= size
            db.executemany('DELETE FROM partitions WHERE path=?', [(p,) for p in gone])
        return len(gone)

    def clear(self, venue=None, market=None, dataset=None, symbol=None) -> int:
        """Drops every partition matching the given fields, all of them when none is given."""
        where = [(c, v) for c, v in (('venue', venue), ('market', market), ('dataset', dataset), ('symbol', symbol)) if v]
        cond = ' AND '.join(f'{c}=?' for c, _ in where) or '1=1'
        args = [v for _, v in where]
        with self._db() as db:
            paths = [p for (p,) in db.execute(f'SELECT path FROM partitions WHERE {cond}', args)]
            db.execute(f'DELETE FROM partitions WHERE {cond}', args)
        for p in paths:
            if p:
                (self.root / p).unlink(missing_ok=True)
        return len(paths)


def _ns(t: pd.Timestamp) -> int:
    return int(pd.Timestamp(t).value)


_default: Cache | None = None


def default() -> Cache:
    """The process-wide cache at the configured root."""
    global _default
    if _default is None:
        _default = Cache()
    return _default
