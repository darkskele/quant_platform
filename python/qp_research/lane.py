"""A research lane, its folder, its spec and its trial ledger.

A primary or registered trial is only recorded once the lane's spec.md is
committed and unchanged since, so a test cannot be named after it has run.
"""
from __future__ import annotations

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
        git = lambda *a: subprocess.run(['git', *a], cwd=self.folder, capture_output=True, text=True)
        if not self.spec.exists() or git('ls-files', '--error-unmatch', 'spec.md').returncode != 0:
            return False
        return git('diff', '--quiet', 'HEAD', '--', 'spec.md').returncode == 0


class GuardedLedger(Ledger):
    """A ledger that refuses primary and registered trials while the lane's spec is not committed."""

    def __init__(self, folder, lane: Lane):
        super().__init__(folder)
        self.lane = lane

    def record(self, returns: pd.Series, name=None, kind='exploratory', params=None) -> pd.Series:
        if kind in ('primary', 'registered') and not self.lane.committed():
            raise SpecNotCommitted(f'{kind} trial {name!r} needs {self.lane.spec} committed and unchanged first')
        return super().record(returns, name, kind, params)
