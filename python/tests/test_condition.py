import numpy as np
import pandas as pd
import pytest

from qp_research import book, condition, costs, panel


def series(n=300, seed=0, gaps=True):
    rng = np.random.default_rng(seed)
    x = pd.Series(rng.normal(size=n), index=pd.date_range('2018-01-07', periods=n, freq='W-SUN'))
    if gaps:
        x.iloc[rng.choice(n, 20, replace=False)] = np.nan
    return x


def loop_thirds(x, min_hist=52):
    out = pd.Series(np.nan, index=x.index)
    for i in range(len(x)):
        past = x.iloc[:i].dropna()
        if len(past) >= min_hist and not np.isnan(x.iloc[i]):
            lo, hi = past.quantile([1 / 3, 2 / 3])
            out.iloc[i] = 0 if x.iloc[i] <= lo else (2 if x.iloc[i] > hi else 1)
    return out


def loop_past_cut(x, q=1 / 3, min_hist=52, below=True):
    out = pd.Series(np.nan, index=x.index)
    for i in range(len(x)):
        past = x.iloc[:i].dropna()
        if len(past) >= min_hist and not np.isnan(x.iloc[i]):
            cut = past.quantile(q)
            out.iloc[i] = float(x.iloc[i] <= cut) if below else float(x.iloc[i] > cut)
    return out


@pytest.mark.parametrize('seed', range(5))
def test_thirds_match_a_loop_over_the_past(seed):
    x = series(seed=seed)
    pd.testing.assert_series_equal(condition.thirds(x), loop_thirds(x), check_dtype=False)


@pytest.mark.parametrize('q,below', [(1 / 3, True), (0.5, False), (0.8, True)])
def test_past_cut_matches_a_loop(q, below):
    x = series(seed=3)
    pd.testing.assert_series_equal(condition.past_cut(x, q, below=below), loop_past_cut(x, q, below=below), check_dtype=False)


def test_thirds_ignore_the_future():
    x = series(gaps=False)
    y = x.copy()
    y.iloc[200:] += 100.0
    pd.testing.assert_series_equal(condition.thirds(x).iloc[:200], condition.thirds(y).iloc[:200])


def test_cells_and_labels():
    x = series(gaps=False)
    cond = pd.DataFrame({'heat': x, 'market direction': np.sign(x)})
    c = condition.cells(cond, x.index)
    assert {'heat low', 'heat mid', 'heat high', 'market direction low', 'market direction high'} == set(c)
    assert not (c['heat low'] & c['heat high']).any()
    assert (c['market direction high'] == (np.sign(x) > 0)).all()


def panel_of(n=500, k=12, seed=0):
    rng = np.random.default_rng(seed)
    rows = []
    for j in range(k):
        closes = 10 * np.exp(np.cumsum(rng.normal(0, 0.04, n)))
        for i, c in enumerate(closes):
            rows.append((f'C{j}', pd.Timestamp('2021-01-04', tz='UTC') + pd.Timedelta(days=i), c, c, c, 2e7))
    long = pd.DataFrame(rows, columns=['symbol', 'day', 'high', 'low', 'close', 'volume'])
    return panel.build(panel.bars(long, warmup=0))


def test_market_return_and_strength():
    p = panel_of()
    m = condition.market_return(p)
    assert np.allclose(m.dropna(), p.returns.mean(axis=1).loc[m.dropna().index])
    s = condition.strength(p)
    i = 30
    expect = abs(m.iloc[i - 4:i].sum()) / (m.iloc[i - 12:i].std() * 2)
    assert s.iloc[i] == pytest.approx(expect)


def test_market_conditions_shape_and_past_only():
    p = panel_of()
    c = condition.market_conditions(p)
    assert list(c.columns) == ['co-movement', 'market vol', 'trend strength', 'funding', 'dispersion', 'market direction']
    assert c['co-movement'].iloc[:12].isna().all() and c['co-movement'].iloc[20:].notna().all()


def test_walk_forward_picks_the_cell_that_paid_before():
    p = panel_of()
    b = book.Book(p, costs.Flat(0.0))
    w = book.inverse_vol(p, np.sign(p.returns.shift(1)))
    idx = b.run(w).index
    good = pd.Series(np.arange(len(idx)) % 2 == 0, index=idx)
    masks = {'good': good, 'bad': ~good}
    oos, mask, picked = condition.walk_forward(b, w, masks, folds=2, history=20, min_on=5)
    assert set(picked.values()) <= {'good', 'bad'}
    assert len(picked) == 2 and mask.index.equals(oos.index)
