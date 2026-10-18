import json

import matplotlib
import numpy as np
import pandas as pd
import pytest

matplotlib.use('Agg')

from qp_research import cv, nb, plot  # noqa: E402


def pooled(n_ts=100, symbols=3, seed=0):
    rng = np.random.default_rng(seed)
    ts = np.repeat(pd.date_range('2022-01-01', periods=n_ts, freq='8h'), symbols)
    return pd.DataFrame({'ts': ts, 'symbol': np.tile(range(symbols), n_ts)}).sample(frac=1, random_state=seed)


def old_splits(events, n_folds=5, horizon=1, embargo=5, min_train_frac=0.3):
    """The funding lane's splitter, kept as the reference."""
    unique_ts = np.sort(events['ts'].unique())
    first = int(len(unique_ts) * min_train_frac)
    size = (len(unique_ts) - first) // n_folds
    to_pos = {t: i for i, t in enumerate(unique_ts)}
    pos = np.array([to_pos[t] for t in events['ts'].to_numpy()])
    for f in range(n_folds):
        lo = first + f * size
        hi = lo + size if f < n_folds - 1 else len(unique_ts)
        test = np.where((pos >= lo) & (pos < hi))[0]
        train = np.where((pos < lo - horizon) | (pos >= hi + embargo + horizon))[0]
        if len(train) and len(test):
            yield train, test


def test_purged_matches_the_old_splitter():
    e = pooled()
    for (a_tr, a_te), (b_tr, b_te) in zip(cv.purged(e), old_splits(e), strict=True):
        assert np.array_equal(a_tr, b_tr) and np.array_equal(a_te, b_te)


def test_purge_and_embargo_keep_train_clear_of_test():
    e = pooled().reset_index(drop=True)
    stamps = np.sort(e.ts.unique())
    for train, test in cv.purged(e, n_folds=4, horizon=2, embargo=3):
        lo, hi = e.ts.iloc[test].min(), e.ts.iloc[test].max()
        i_lo, i_hi = np.searchsorted(stamps, lo), np.searchsorted(stamps, hi)
        tr = np.searchsorted(stamps, e.ts.iloc[train].to_numpy())
        assert not ((tr >= i_lo - 2) & (tr <= i_hi + 3 + 2)).any()
        assert set(e.ts.iloc[test]) == set(stamps[i_lo:i_hi + 1])


def test_purged_folds_tile_the_tail():
    e = pooled(n_ts=103)
    tests = [t for _, t in cv.purged(e, n_folds=5, min_train_frac=0.3)]
    covered = np.sort(np.concatenate(tests))
    assert len(covered) == len(set(covered))
    assert e.ts.iloc[covered].nunique() == 103 - int(103 * 0.3)


def test_purged_refuses_bad_input():
    with pytest.raises(ValueError):
        list(cv.purged(pd.DataFrame({'x': [1]})))
    with pytest.raises(ValueError):
        list(cv.purged(pooled(n_ts=4), n_folds=5))


def test_halves_is_the_first_three_fifths():
    h = cv.halves(pd.RangeIndex(100))
    assert h.iloc[:60].all() and not h.iloc[60:].any()


def test_style_and_diverging():
    plot.style()
    assert matplotlib.rcParams['axes.spines.top'] is False
    cmap = plot.diverging()
    assert matplotlib.colors.to_hex(cmap(0.0)) == plot.ORANGE
    assert matplotlib.colors.to_hex(cmap(1.0)) == plot.BLUE


def test_build_executes_and_embeds_outputs(tmp_path):
    path = nb.build(tmp_path / 'probe.ipynb', [nb.md('# Probe', '', 'A line.'), nb.code('x = 2 + 3', 'print(x)')])
    doc = json.loads(path.read_text())
    assert doc['metadata']['kernelspec']['display_name'] == 'qp-research'
    assert ''.join(doc['cells'][0]['source']) == '# Probe\n\nA line.'
    assert ''.join(doc['cells'][1]['outputs'][0]['text']).strip() == '5'


def test_execute_raises_on_a_failed_cell(tmp_path):
    path = nb.write(tmp_path / 'bad.ipynb', [nb.code('raise ValueError("boom")')])
    with pytest.raises(RuntimeError, match='ValueError: boom'):
        nb.execute(path)
    assert nb.errors(path) == ['ValueError: boom']
