import numpy as np
import pandas as pd
import pytest

from qp_research import stats


def weekly(values, start='2021-01-03'):
    return pd.Series(values, index=pd.date_range(start, periods=len(values), freq='W-SUN'), dtype=float)


def alternating(mean, spread, n):
    """n values with population mean and standard deviation exactly mean and spread."""
    return mean + spread * np.tile([1.0, -1.0], n // 2)


@pytest.mark.parametrize('freq,expected', [('W-SUN', 52), ('D', 365), ('h', 8760)])
def test_periods_inferred_from_spacing(freq, expected):
    assert stats.periods_per_year(pd.date_range('2021-01-03', periods=10, freq=freq, tz='UTC')) == expected


def test_periods_inferred_through_gaps():
    idx = pd.date_range('2021-01-03', periods=10, freq='W-SUN').delete([3, 7])
    assert stats.periods_per_year(idx) == 52


def test_periods_refused_without_a_known_spacing():
    with pytest.raises(ValueError):
        stats.periods_per_year(pd.date_range('2021-01-01', periods=10, freq='2D'))
    with pytest.raises(ValueError):
        stats.sharpe(np.array([0.01, 0.02, -0.01, 0.03]))


def test_sharpe_known_answer():
    x = weekly(alternating(0.01, 0.02, 100))
    assert stats.sharpe(x) == pytest.approx(0.5 * np.sqrt(52))
    assert stats.sharpe(x.values, periods=365) == pytest.approx(0.5 * np.sqrt(365))


def test_sharpe_drops_nan_and_refuses_degenerate():
    x = weekly(alternating(0.01, 0.02, 100))
    x.iloc[[5, 6]] = np.nan
    x = x.drop(x.index[[4, 7]])
    assert stats.sharpe(x) == pytest.approx(0.5 * np.sqrt(52))
    assert np.isnan(stats.sharpe(weekly([0.01] * 10)))
    assert np.isnan(stats.sharpe(weekly([0.01, 0.02])))


def test_annual_return_and_volatility():
    x = weekly(alternating(0.01, 0.02, 100))
    assert stats.annual_return(x) == pytest.approx(0.52)
    assert stats.volatility(x) == pytest.approx(0.02 * np.sqrt(52))


def test_drawdown_by_hand():
    assert stats.drawdown(weekly([0.1, -0.05, -0.1, 0.2])) == pytest.approx(-0.15)
    assert stats.drawdown(weekly([-0.1, 0.05, -0.1])) == pytest.approx(-0.15)
    assert stats.drawdown(weekly([0.1, 0.1])) == 0.0


def test_t_stat_known_answer():
    x = alternating(0.01, 0.02, 100)
    assert stats.t_stat(x) == pytest.approx(0.01 / x.std(ddof=1) * 10)


def test_by_year_splits_calendar_years():
    x = pd.concat([weekly(alternating(0.01, 0.02, 52), '2021-01-03'), weekly(alternating(-0.01, 0.02, 52), '2022-01-02')])
    y = stats.by_year(x)
    assert list(y.index) == [2021, 2022]
    assert y[2021] == pytest.approx(0.5 * np.sqrt(52))
    assert y[2022] == pytest.approx(-0.5 * np.sqrt(52))
    frame = stats.by_year(pd.DataFrame({'a': x, 'b': -x}))
    assert frame.shape == (2, 2)
    assert frame.loc['b', 2021] == pytest.approx(-0.5 * np.sqrt(52))
    assert stats.by_year(x, lambda g: g.sum())[2021] == pytest.approx(0.52)


def test_summary_fields():
    x = pd.concat([weekly(alternating(0.01, 0.02, 52), '2021-01-03'), weekly(alternating(-0.01, 0.02, 52), '2022-01-02')])
    s = stats.summary(x)
    assert s['periods'] == 104
    assert s['years positive'] == 0.5
    assert s['sharpe'] == pytest.approx(0.0, abs=1e-12)
    assert stats.summary(pd.DataFrame({'a': x, 'b': x})).shape == (2, 6)


def test_ic_perfect_ranks_and_name_floor():
    rng = np.random.default_rng(0)
    idx = pd.date_range('2021-01-03', periods=5, freq='W-SUN')
    sig = pd.DataFrame(rng.normal(size=(5, 30)), index=idx)
    fwd = sig ** 3
    fwd.iloc[1] = -fwd.iloc[1]
    fwd.iloc[2, 15:] = np.nan
    out = stats.ic(sig, fwd)
    assert list(out.index) == list(idx[[0, 1, 3, 4]])
    assert out.iloc[0] == pytest.approx(1.0)
    assert out.iloc[1] == pytest.approx(-1.0)
    assert len(stats.ic(sig, fwd, min_names=10)) == 5


def test_cluster_ols_matches_statsmodels():
    sm = pytest.importorskip('statsmodels.api')
    rng = np.random.default_rng(1)
    groups = np.repeat(np.arange(40), 25)
    shock = rng.normal(size=40)[groups]
    X = np.column_stack([np.ones(len(groups)), rng.normal(size=len(groups)) + shock])
    y = X @ np.array([0.1, 0.5]) + shock + rng.normal(size=len(groups))
    beta, se = stats.cluster_ols(y, X, groups)
    ref = sm.OLS(y, X).fit(cov_type='cluster', cov_kwds={'groups': groups})
    np.testing.assert_allclose(beta, ref.params)
    np.testing.assert_allclose(se, ref.bse)
