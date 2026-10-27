"""Train and test splits that respect time.

Walk forward fixes the time direction. Purge drops train rows whose label
window overlaps the test span, and embargo drops train rows just after it in
case features spill forward.
"""
from __future__ import annotations

from typing import Iterator

import numpy as np
import pandas as pd


def purged(events: pd.DataFrame, n_folds=5, horizon=1, embargo=5, min_train_frac=0.3) -> Iterator[tuple[np.ndarray, np.ndarray]]:
    """(train, test) positional indices into events, walking forward over its distinct ts values.

    Test folds are equal contiguous slices of distinct stamps from
    min_train_frac onwards. Train is every row before the test span less
    horizon stamps, and every row from embargo plus horizon stamps after it.
    """
    if 'ts' not in events.columns:
        raise ValueError("events needs a 'ts' column")
    if n_folds < 2:
        raise ValueError('n_folds must be at least 2')
    stamps, pos = np.unique(events['ts'].to_numpy(), return_inverse=True)
    first = int(len(stamps) * min_train_frac)
    size = (len(stamps) - first) // n_folds
    if size < 1:
        raise ValueError('too few distinct stamps for n_folds and min_train_frac')
    for f in range(n_folds):
        lo = first + f * size
        hi = lo + size if f < n_folds - 1 else len(stamps)
        test = np.where((pos >= lo) & (pos < hi))[0]
        train = np.where((pos < lo - horizon) | (pos >= hi + embargo + horizon))[0]
        if len(train) and len(test):
            yield train, test


def halves(idx: pd.Index) -> pd.Series:
    """True on the first three of five equal blocks of periods."""
    train = pd.Series(False, index=idx)
    train.iloc[np.concatenate(np.array_split(np.arange(len(idx)), 5)[:3])] = True
    return train
