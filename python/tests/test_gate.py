import numpy as np
import pandas as pd
import pytest
from scipy import stats as sps

from qp_research import calibrate, gate


def weekly(values, start='2020-01-05'):
    return pd.Series(values, index=pd.date_range(start, periods=len(values), freq='W-SUN'), dtype=float)


def alternating(mean, spread, n):
    """n values with population mean and standard deviation exactly mean and spread, no skew, kurtosis 1."""
    return mean + spread * np.tile([1.0, -1.0], n // 2)


def noise(seed, n=338, sr=0.0):
    rng = np.random.default_rng(seed)
    return weekly(rng.normal(sr / np.sqrt(52) * 0.02, 0.02, n))


def test_expected_max_known_values():
    assert gate.expected_max(1) == 0.0
    assert gate.expected_max(2) == pytest.approx(1 / np.sqrt(np.pi))


def test_effective_trials_counts_copies_once_and_independents_in_full():
    rng = np.random.default_rng(0)
    base = rng.normal(size=500)
    copies = pd.DataFrame({i: base + 1e-6 * rng.normal(size=500) for i in range(10)})
    independent = pd.DataFrame(rng.normal(size=(500, 5)))
    assert gate.effective_trials(copies) == pytest.approx(1.0, abs=0.1)
    assert gate.effective_trials(independent) == pytest.approx(5.0, rel=0.1)
    assert gate.effective_trials(copies, extra=4) == pytest.approx(5.0, rel=0.1)


def test_significance_known_answer():
    x = weekly(alternating(0.01, 0.02, 300))
    prob, bar = gate.significance(x, 1.0)
    assert prob == pytest.approx(sps.norm.cdf(0.5 * np.sqrt(299)))
    assert bar == 0.0
    _, bar10 = gate.significance(x, 10.0)
    assert bar10 == pytest.approx(gate.expected_max(10.0) / np.sqrt(299) * np.sqrt(52))


def test_more_trials_lower_the_probability():
    x = noise(1, sr=1.0)
    probs = [gate.significance(x, t)[0] for t in (1, 10, 100)]
    assert probs[0] > probs[1] > probs[2]


def test_periods_needed_matches_the_calibration():
    assert [gate.periods_needed(sr, 1.0, 52) for sr in (0.5, 0.8, 1.0, 1.5)] == [1287, 504, 323, 144]
    assert gate.periods_needed(0.0, 1.0, 52) == -1


def test_year_stability_passes_spread_edge_and_fails_one_year():
    years = [alternating(0.01, 0.02, 52) for _ in range(6)]
    assert gate.year_stability(weekly(np.concatenate(years)))[0]
    years = [alternating(0.0, 0.02, 52) for _ in range(5)] + [alternating(0.06, 0.02, 52)]
    ok, kept = gate.year_stability(weekly(np.concatenate(years)))
    assert not ok and kept < 0.4
    assert gate.year_stability(weekly(alternating(-0.01, 0.02, 312))) == (False, pytest.approx(np.nan, nan_ok=True))


def test_year_stability_on_arrays_uses_blocks():
    years = [alternating(0.01, 0.02, 52) for _ in range(6)]
    assert gate.year_stability(np.concatenate(years), periods=52)[0]


def test_oos_agree():
    x = alternating(0.01, 0.02, 200)
    assert gate.oos_agree(x, x) == (True, 0.0)
    ok, z = gate.oos_agree(x, -x)
    assert not ok and z > gate.Z


def test_neighbours_agree():
    best = noise(2, sr=1.0)
    ok, zs = gate.neighbours_agree(best, [best, best])
    assert ok and zs == [pytest.approx(0.0), pytest.approx(0.0)]
    assert not gate.neighbours_agree(best, [best, -best])[0]


def test_pbo_separates_information_from_luck():
    rng = np.random.default_rng(3)
    informative = pd.DataFrame(rng.normal(0, 0.02, (520, 10)))
    informative[0] += 0.01
    assert gate.pbo(informative, splits=8) < 0.05
    luck = [gate.pbo(pd.DataFrame(np.random.default_rng(s).normal(0, 0.02, (520, 10))), splits=8) for s in range(10)]
    assert 0.3 < np.mean(luck) < 0.7


def test_walk_forward_picks_on_the_past_only():
    rng = np.random.default_rng(4)
    books = pd.DataFrame(rng.normal(0, 0.02, (400, 3)), index=pd.date_range('2018-01-07', periods=400, freq='W-SUN'),
                         columns=['a', 'b', 'c'])
    books['b'] += 0.01
    edges = np.linspace(104, 400, 7).astype(int)
    seen = []

    def pick(past):
        seen.append(past.index[-1])
        return past.apply(lambda c: c.mean()).idxmax()

    oos, picked = gate.walk_forward(books, pick=pick)
    assert picked == ['b'] * 6
    assert oos.index[0] == books.index[104] and oos.index[-1] == books.index[-1]
    assert seen == [books.index[lo - 1] for lo in edges[:-1]]
    assert gate.walk_forward(books)[1] == ['b'] * 6


def test_bootstrap_draws_the_same_paths_as_a_plain_loop():
    x = noise(5, sr=1.0)
    v, block, n = x.values, 8, 200
    starts = np.random.default_rng(0).integers(0, len(v) - block + 1, size=(n, int(np.ceil(len(v) / block))))
    paths = np.stack([np.concatenate([v[a:a + block] for a in s])[:len(v)] for s in starts])
    sr = paths.mean(1) / paths.std(1) * np.sqrt(52)
    out = gate.bootstrap(x, block=block, n=n)
    assert out['sharpe 5%'] == pytest.approx(np.percentile(sr, 5))
    assert out['ends above zero'] == pytest.approx((paths.sum(1) > 0).mean())


@pytest.mark.parametrize('failed,prob,exploratory,open_items,expected', [
    ([], 0.97, False, (), 'gate pass'),
    ([], 0.97, True, (), 'gate park, needs a forward run'),
    ([], 0.7, False, ('T5',), 'gate park, needs about 504 periods, open T5'),
    (['S2'], 0.97, False, (), 'gate fail S2'),
    (['S2'], 0.3, False, (), 'gate fail S1 S2'),
    ([], 0.3, False, (), 'gate fail S1'),
])
def test_stamp(failed, prob, exploratory, open_items, expected):
    assert gate.stamp(failed, prob, 0.8, 1.0, 52, open_items, exploratory) == expected


def test_score_names_unrun_checks_open():
    x = weekly(np.concatenate([alternating(0.02, 0.02, 52) for _ in range(7)]))
    v = gate.score(x, 1.0)
    assert v.stamp == 'gate pass, open S3 S4 S5'
    assert v.open == ['S3', 'S4', 'S5'] and v.failed == []
    assert v.sharpe == pytest.approx(np.sqrt(52))


def test_score_runs_given_checks_and_caps_exploratory():
    x = weekly(np.concatenate([alternating(0.02, 0.02, 52) for _ in range(7)]))
    v = gate.score(x, 1.0, oos=(x, x), neighbours=[x], exploratory=True)
    assert v.stamp == 'gate park, needs a forward run, open S5'
    assert v.numbers['S3'] == 0.0
    v = gate.score(x, 1.0, oos=(x, -x))
    assert v.stamp.startswith('gate fail S3')


def test_score_fails_a_one_year_edge():
    x = weekly(np.concatenate([alternating(0.0, 0.02, 52) for _ in range(6)] + [alternating(0.08, 0.02, 52)]))
    v = gate.score(x, 1.0)
    assert 'S2' in v.failed and v.stamp.startswith('gate fail')


def test_calibration_holds():
    e = calibrate.edges(338, 52, n=300).set_index(['true sharpe', 'edge'])
    assert e.loc[(0.0, 'none'), 'S1 one test'] < 0.12
    assert e.loc[(1.5, 'stationary'), 'S1 one test'] > 0.85
    assert e.loc[(1.0, 'stationary'), 'S2'] > 0.85
    assert e.loc[(1.0, 'one year'), 'S2'] < 0.25
    g = calibrate.grids(338, 52, n=40)
    assert (g['deflated'] < 0.15).all()
    assert g['one test'].iloc[-1] > g['deflated'].iloc[-1]
    p = calibrate.plateau(338, 52, n=300)
    assert p['spike 0.8'] < 0.05 and p['plateau 0.8'] > 0.7
    assert list(calibrate.power(52)) == [1287, 504, 323, 144]


def test_calibration_volatility_is_set_per_year():
    rng = np.random.default_rng(0)
    weekly_paths = calibrate.returns(rng, 0.0, 2000, 338, 52)
    daily_paths = calibrate.returns(rng, 0.0, 200, 2372, 365)
    assert weekly_paths.std() == pytest.approx(daily_paths.std(), rel=0.05)
    one_year = calibrate.returns(rng, 1.0, 500, 2372, 365, concentrated=True)
    assert one_year.mean() / one_year.std() * np.sqrt(365) == pytest.approx(1.0, abs=0.15)
