"""The trial ledger, every book a lane runs, kept as its return series.

A trial is keyed by its name and settings. Recording the same key again
replaces it, and recording identical returns writes nothing. Records are
buffered and written as append-only zstd parquet files in the ledger folder,
so a lane's ledger is committed beside its notebooks and only grows when a
result does.

Kinds are primary, exploratory, registered and check. A check, such as a
random-sign or cost-sensitivity run, is kept but never counted as a trial.
"""
from __future__ import annotations

import atexit
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

import numpy as np
import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq

from . import gate

KINDS = ('primary', 'exploratory', 'registered', 'check')
FLUSH = 200   # buffered trials written per file


def _commit(folder: Path) -> str:
    """The checked-out commit, empty outside a git repo."""
    run = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], cwd=folder, capture_output=True, text=True)
    return run.stdout.strip() if run.returncode == 0 else ''


def _digest(r: pd.Series) -> str:
    v = r.astype('float32')
    return hashlib.sha1(np.ascontiguousarray(v.index.asi8).tobytes() + v.to_numpy().tobytes()).hexdigest()


class Ledger:
    """One lane's trials, read from and appended to a folder of parquet files."""

    def __init__(self, folder):
        self.folder = Path(folder)
        self._buffer: dict[str, pd.DataFrame] = {}
        self._known: dict[str, str] | None = None
        atexit.register(self.save)

    def _index(self) -> dict[str, str]:
        """Each recorded key's latest returns digest."""
        if self._known is None:
            t = self.table()
            self._known = dict(zip(t.key, t.digest)) if len(t) else {}
        return self._known

    def record(self, returns: pd.Series, name: str | None = None, kind='exploratory', params: dict | None = None) -> pd.Series:
        """Adds a trial and hands the returns back. Unnamed trials are keyed by their returns."""
        if kind not in KINDS:
            raise ValueError(f'kind must be one of {KINDS}')
        r = returns.dropna()
        digest = _digest(r)
        key = name if name is not None else f'unnamed {digest[:12]}'
        if params:
            key += ' ' + json.dumps(params, sort_keys=True, default=str)
        if self._index().get(key) == digest:
            return returns
        self._known[key] = digest
        self._buffer[key] = (pd.DataFrame({'key': key, 'name': name or '', 'kind': kind, 'params': json.dumps(params or {}, sort_keys=True, default=str),
                                          'digest': digest, 't': r.index, 'r': r.to_numpy(dtype='float32')}))
        if len(self._buffer) >= FLUSH:
            self.save()
        return returns

    def save(self) -> Path | None:
        """Writes buffered trials to a new file. Nothing to write returns None."""
        if not self._buffer:
            return None
        self.folder.mkdir(parents=True, exist_ok=True)
        rows = pd.concat(self._buffer.values(), ignore_index=True)
        rows['commit'] = _commit(self.folder)
        rows['recorded'] = pd.Timestamp.now(tz='UTC')
        # Named by write time in nanoseconds, so the files sort in the order they were written.
        path = self.folder / f'{time.time_ns():020d}-{os.getpid()}.parquet'
        # Single threaded, since the last save runs at exit after thread pools have stopped.
        pq.write_table(pa.Table.from_pandas(rows, preserve_index=False, nthreads=1), path, compression='zstd')
        self._buffer = {}
        return path

    def _rows(self) -> pd.DataFrame:
        files = sorted(self.folder.glob('*.parquet'))
        parts = [pd.read_parquet(f).assign(file=i) for i, f in enumerate(files)] + [b.assign(file=len(files)) for b in self._buffer.values()]
        if not parts:
            return pd.DataFrame(columns=['key', 'name', 'kind', 'params', 'digest', 't', 'r', 'file'])
        rows = pd.concat(parts, ignore_index=True)
        # The latest file holding a key replaces every earlier record of it.
        latest = rows.groupby('key').file.transform('max')
        return rows[rows.file == latest]

    def table(self) -> pd.DataFrame:
        """One row per trial, its name, kind, settings, span and length."""
        rows = self._rows()
        if not len(rows):
            return pd.DataFrame(columns=['key', 'name', 'kind', 'params', 'digest', 'first', 'last', 'periods'])
        return rows.groupby('key', sort=False).agg(name=('name', 'first'), kind=('kind', 'first'), params=('params', 'first'),
                                                   digest=('digest', 'first'), first=('t', 'min'), last=('t', 'max'),
                                                   periods=('r', 'size')).reset_index()

    def frame(self, kinds=('primary', 'exploratory', 'registered')) -> pd.DataFrame:
        """Periods by trials, the returns of every trial of the given kinds."""
        rows = self._rows()
        rows = rows[rows.kind.isin(kinds)]
        if not len(rows):
            return pd.DataFrame()
        return rows.pivot_table(index='t', columns='key', values='r', aggfunc='last').sort_index()

    def trials(self, kinds=('primary', 'exploratory', 'registered'), extra=0) -> float:
        """How many independent tests the trials of the given kinds are worth. extra adds trials with no series."""
        f = self.frame(kinds)
        if f.shape[1] == 0:
            return float(max(extra, 1))
        return gate.effective_trials(f, extra=extra)
