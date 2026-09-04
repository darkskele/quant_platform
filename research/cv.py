"""Purged, embargoed walk forward CV for the funding signal loop.

Standard k fold leaks with autocorrelated series and overlapping label
windows. Walk forward fixes the time direction, purge drops train rows whose
label window overlaps the test window, embargo drops train rows immediately
after a test window in case features spill forward.

Prediction at row t targets funding over [ts_t, ts_{t+1}], so
one 8h interval. Purge is symmetric around the test span by `horizon`.
Embargo `k` drops the next k intervals after each test span from training.

Assumes a pooled cross-symbol frame ordered by ts. Symbol identity is not
used by the splitter; a row is a row.
"""

from __future__ import annotations

from typing import Iterator

import numpy as np
import pandas as pd


def walk_forward_splits(
    events: pd.DataFrame,
    n_folds: int = 5,
    horizon: int = 1,
    embargo: int = 5,
    min_train_frac: float = 0.3,
) -> Iterator[tuple[np.ndarray, np.ndarray]]:
    """Yield (train_idx, test_idx) as positional indices into `events`.

    Rows are ordered by `ts`; ties broken by symbol for determinism. Test
    folds are equal-sized contiguous slices of unique ts values from
    `min_train_frac` onwards, so each fold has both a real train history and
    a real test span.
    """
    if "ts" not in events.columns:
        raise ValueError("events must have a 'ts' column")
    if n_folds < 2:
        raise ValueError("n_folds must be >= 2")

    unique_ts = np.sort(events["ts"].unique())

    n_ts = len(unique_ts)
    first_test_ts_idx = int(n_ts * min_train_frac)
    testable = n_ts - first_test_ts_idx
    fold_size = testable // n_folds
    if fold_size < 1:
        raise ValueError("not enough distinct ts values for the requested n_folds/min_train_frac")

    ts_to_pos = {t: i for i, t in enumerate(unique_ts)}
    row_ts_idx = np.array([ts_to_pos[t] for t in events["ts"].to_numpy()])

    for f in range(n_folds):
        lo = first_test_ts_idx + f * fold_size
        hi = lo + fold_size if f < n_folds - 1 else n_ts
        test_mask = (row_ts_idx >= lo) & (row_ts_idx < hi)
        # Train: everything strictly before the purged left edge, or strictly
        # after the embargoed right edge (with a symmetric purge for horizon).
        left_purge = lo - horizon
        right_embargo = hi + embargo + horizon
        train_mask = (row_ts_idx < left_purge) | (row_ts_idx >= right_embargo)

        train_idx = np.where(train_mask)[0]
        test_idx = np.where(test_mask)[0]
        if len(train_idx) == 0 or len(test_idx) == 0:
            continue
        yield train_idx, test_idx


def fold_report(pnls: list[pd.DataFrame], metrics: list[dict]) -> pd.DataFrame:
    """Assemble per-fold metrics into one frame for easy comparison."""
    rows = []
    for i, m in enumerate(metrics):
        rows.append({"fold": i, **m})
    return pd.DataFrame(rows)
