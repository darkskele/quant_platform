"""A research lane, its folder, its spec and its trial ledger.

A primary or registered trial is only recorded once the lane's spec.md is
committed and unchanged since, so a test cannot be named after it has run.

A copy of the repo without .git, as on the build box, answers from the
.git-head file at its root instead: git ls-tree -r HEAD taken at sync time.
"""
from __future__ import annotations

import hashlib
import subprocess
from pathlib import Path

import pandas as pd

from .ledger import Ledger


class SpecNotCommitted(RuntimeError):
    pass


class Lane:
    """The lane in folder, with its ledger in folder/ledger."""

    def __init__(self, folder):
        self.folder = Path(folder).resolve()
        self.spec = self.folder / 'spec.md'
        self.ledger = GuardedLedger(self.folder / 'ledger', self)

    def committed(self) -> bool:
        """True when spec.md is tracked and has no uncommitted change."""
        if not self.spec.exists():
            return False
        git = lambda *a: subprocess.run(['git', *a], cwd=self.folder, capture_output=True, text=True)
        if git('rev-parse', '--git-dir').returncode != 0:
            return _in_head_tree(self.spec)
        if git('ls-files', '--error-unmatch', 'spec.md').returncode != 0:
            return False
        return git('diff', '--quiet', 'HEAD', '--', 'spec.md').returncode == 0


def _in_head_tree(path: Path) -> bool:
    """True when path's content is the blob .git-head names for it."""
    root = next((d for d in path.parents if (d / '.git-head').exists()), None)
    if root is None:
        return False
    rel = path.relative_to(root).as_posix()
    data = path.read_bytes()
    blob = hashlib.sha1(b'blob %d\0' % len(data) + data).hexdigest()
    # Each line is "<mode> blob <sha>\t<path>".
    for line in (root / '.git-head').read_text().splitlines():
        meta, _, name = line.partition('\t')
        if name == rel:
            return meta.split()[2] == blob
    return False


class GuardedLedger(Ledger):
    """A ledger that refuses primary and registered trials while the lane's spec is not committed."""

    def __init__(self, folder, lane: Lane):
        super().__init__(folder)
        self.lane = lane

    def record(self, returns: pd.Series, name=None, kind='exploratory', params=None) -> pd.Series:
        if kind in ('primary', 'registered') and not self.lane.committed():
            raise SpecNotCommitted(f'{kind} trial {name!r} needs {self.lane.spec} committed and unchanged first')
        return super().record(returns, name, kind, params)
