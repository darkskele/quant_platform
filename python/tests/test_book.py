import numpy as np
import pandas as pd
import pytest

from qp_research import book, costs, panel


def ts(s):
    return pd.Timestamp(s, tz='UTC')


def long_bars(closes: dict, start='2023-01-02') -> pd.DataFrame:
    rows = []
    for sym, cs in closes.items():
        for i, c in enumerate(cs):
            traded = not np.isnan(c)
            rows.append((sym, ts(start) + pd.Timedelta(days=i), c, c, c if traded else 0.0, 2e7 if traded else 0.0))
    return pd.DataFrame(rows, columns=['symbol', 'day', 'high', 'low', 'close', 'volume'])


def random_bars(n=400, k=6, seed=0):
    rng = np.random.default_rng(seed)
    return long_bars({f'C{j}': list(10 * np.exp(np.cumsum(rng.normal(0, 0.04, n)))) for j in range(k)})


def known():
    """Two coins over three full weeks with known weekly simple returns and funding."""
    a = [100.0] * 7 + [110.0] * 7 + [99.0] * 7 + [99.0] * 7
    b = [50.0] * 7 + [50.0] * 7 + [55.0] * 7 + [55.0] * 7
    bars = long_bars({'A': a, 'B': b})
    prints = pd.DataFrame({'symbol': ['A', 'B'], 't': [ts('2023-01-17 08:00'), ts('2023-01-24 08:00')],
                           'funding_rate': [0.001, 0.002]})
    return panel.build(panel.bars(bars, prints, warmup=0), min_bars=1)


def test_pnl_by_hand():
    p = known()
    w = pd.DataFrame(0.0, index=p.returns.index, columns=['A', 'B'])
    w.loc[ts('2023-01-15'):, 'A'] = 0.5
    w.loc[ts('2023-01-15'):, 'B'] = -0.5
    r = book.Book(p, costs.Flat(10.0)).run(w)
    assert r.index[0] == ts('2023-01-15')
    # Week to 01-15: A 100 to 110, B flat. Week to 01-22: A 110 to 99, B 50 to 55.
    assert r.gross.loc[ts('2023-01-15')] == pytest.approx(0.5 * 0.10)
    assert r.gross.loc[ts('2023-01-22')] == pytest.approx(0.5 * -0.10 - 0.5 * 0.10)
    assert r.funding.loc[ts('2023-01-22')] == pytest.approx(-0.5 * 0.001)
    assert r.funding.loc[ts('2023-01-29')] == pytest.approx(0.5 * 0.002)
    assert r.cost.loc[ts('2023-01-15')] == pytest.approx(-1.0 * 10 / 1e4)
    assert (r.cost.iloc[1:] == 0).all()
    assert (r.net == r.gross + r.funding + r.cost).all()


def test_cost_only_on_a_weight_change_and_scaled():
    p = known()
    w = pd.DataFrame(0.0, index=p.returns.index, columns=['A', 'B'])
    w['A'] = [0.0, 1.0, -1.0, -1.0]
    b = book.Book(p, costs.Flat(10.0))
    r = b.run(w)
    assert list(r.turnover) == [1.0, 2.0, 0.0]
    assert list(r.cost) == pytest.approx([-0.001, -0.002, 0.0])
    assert list(b.run(w, cost_mult=2.0).cost) == pytest.approx([-0.002, -0.004, 0.0])


def test_halted_coin_keeps_its_weight():
    closes = [100.0] * 13 + [np.nan] * 2 + [100.0] * 13
    p = panel.build(panel.bars(long_bars({'A': closes, 'B': [100.0] * 28}), warmup=0), min_bars=1)
    w = pd.DataFrame({'A': [1.0, 1.0, 0.0, 0.0], 'B': 0.0}, index=p.returns.index)
    held = book.Book(p, costs.Flat(0.0)).hold(w)
    assert list(held.A) == [1.0, 1.0, 1.0, 0.0]


def test_inverse_vol_weights():
    p = panel.build(panel.bars(random_bars(), warmup=0))
    sig = np.sign(p.returns.shift(1))
    w = book.inverse_vol(p, sig)
    live = w.abs().sum(axis=1) > 0
    assert np.allclose(w[live].abs().sum(axis=1), 1.0)
    row = w[live].iloc[-1]
    expect = (sig.loc[row.name] / p.vol.loc[row.name])
    assert np.allclose(row, expect / expect.abs().sum())
    assert w.where(p.liquidity < 1e7).abs().sum().sum() == 0


def test_sleeve_books_exit_cost_to_the_last_on_period():
    p = known()
    w = pd.DataFrame({'A': 1.0, 'B': 0.0}, index=p.returns.index)
    on = pd.Series([False, True, True, False], index=p.returns.index)
    b = book.Book(p, costs.Flat(10.0))
    s = b.sleeve(w, on)
    assert list(s.index) == list(p.returns.index[1:3])
    gated = b.run(book.gated(w, on))
    assert s.iloc[-1] == pytest.approx(gated.net.loc[s.index[-1]] + gated.cost.loc[p.returns.index[3]])


def test_by_bar_sums_to_the_period_book():
    p = panel.build(panel.bars(random_bars(), warmup=0))
    b = book.Book(p, costs.for_market('usdm', 1e5))
    w = book.inverse_vol(p, np.sign(p.returns.shift(1)))
    r = b.run(w)
    daily = b.by_bar(w).resample('W-SUN').sum().loc[r.index[0]:]
    for c in ('gross', 'funding', 'cost', 'turnover', 'net'):
        assert np.allclose(daily[c].reindex(r.index), r[c])


@pytest.mark.parametrize('rule', ['W-SUN', 'W-WED'])
def test_one_slice_is_the_plain_book(rule):
    bars = panel.bars(random_bars(), warmup=0)
    cost = costs.for_market('usdm', 1e5)
    weights = lambda p: book.inverse_vol(p, np.sign(p.returns.shift(1)))
    s = book.Sliced(bars, cost, periods=[rule], report=rule).run(weights)
    p = panel.build(bars, rule)
    r = book.Book(p, cost).run(weights(p))
    assert np.allclose(s.net, r.net.reindex(s.index))


def test_sliced_book_never_looks_ahead():
    rows = random_bars()
    cut = ts('2023-09-01')
    later = rows.day >= cut
    shocked = rows.copy()
    shocked.loc[later, 'close'] *= np.exp(np.random.default_rng(9).normal(0, 0.2, later.sum()))
    weights = lambda p: book.inverse_vol(p, np.sign(p.returns.shift(1)))
    cost = costs.for_market('usdm', 1e5)
    a = book.Sliced(panel.bars(rows, warmup=0), cost).run(weights)
    b = book.Sliced(panel.bars(shocked, warmup=0), cost).run(weights)
    before = a.index[a.index < cut - pd.Timedelta(days=7)]
    pd.testing.assert_frame_equal(a.loc[before], b.loc[before])


def test_equal_risk_scales_by_prior_volatility():
    idx = pd.date_range('2020-01-05', periods=120, freq='W-SUN')
    rng = np.random.default_rng(1)
    a = pd.Series(rng.normal(0, 0.01, 120), index=idx)
    out = book.equal_risk(a, a * 4)
    assert np.allclose(out, (a / a.rolling(52, min_periods=26).std().shift(1)).dropna())
