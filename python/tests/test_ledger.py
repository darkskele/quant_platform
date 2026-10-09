import subprocess

import numpy as np
import pandas as pd
import pytest

from qp_research import book, costs, gate, ledger as ledger_, panel
from qp_research.lane import Lane, SpecNotCommitted


def weekly(values, start='2021-01-03'):
    return pd.Series(values, index=pd.date_range(start, periods=len(values), freq='W-SUN', tz='UTC'), dtype=float)


def noise(seed, n=200):
    return weekly(np.random.default_rng(seed).normal(0, 0.02, n))


def test_record_save_and_read_back(tmp_path):
    led = ledger_.Ledger(tmp_path)
    led.record(noise(0), 'per coin', params={'L': 4})
    led.record(noise(1), 'market', kind='primary')
    led.save()
    back = ledger_.Ledger(tmp_path)
    t = back.table().set_index('key')
    assert set(t.index) == {'per coin {"L": 4}', 'market'}
    assert t.loc['market', 'kind'] == 'primary' and t.loc['market', 'periods'] == 200
    f = back.frame()
    assert f.shape == (200, 2)
    np.testing.assert_allclose(f['market'], noise(1).astype('float32'))


def test_identical_rerun_writes_nothing(tmp_path):
    led = ledger_.Ledger(tmp_path)
    led.record(noise(0), 'a')
    led.save()
    again = ledger_.Ledger(tmp_path)
    again.record(noise(0), 'a')
    assert again.save() is None
    assert len(list(tmp_path.glob('*.parquet'))) == 1


def test_same_key_keeps_the_latest(tmp_path):
    led = ledger_.Ledger(tmp_path)
    led.record(noise(0), 'a')
    led.save()
    led2 = ledger_.Ledger(tmp_path)
    led2.record(noise(5), 'a')
    led2.record(noise(6), 'a')
    led2.save()
    t = ledger_.Ledger(tmp_path).table()
    assert len(t) == 1 and t.periods.iloc[0] == 200
    np.testing.assert_allclose(ledger_.Ledger(tmp_path).frame()['a'], noise(6).astype('float32'))


def test_unnamed_trials_are_keyed_by_returns(tmp_path):
    led = ledger_.Ledger(tmp_path)
    led.record(noise(0))
    led.record(noise(0))
    led.record(noise(1))
    assert len(led.table()) == 2
    assert all(k.startswith('unnamed ') for k in led.table().key)


def test_checks_are_kept_but_never_counted(tmp_path):
    led = ledger_.Ledger(tmp_path)
    led.record(noise(0), 'real')
    for i in range(5):
        led.record(noise(10 + i), f'random {i}', kind='check')
    assert len(led.table()) == 6
    assert list(led.frame().columns) == ['real']
    assert led.trials() == pytest.approx(1.0, abs=0.05)
    with pytest.raises(ValueError):
        led.record(noise(0), 'x', kind='guess')


def test_trials_count_copies_once_and_independents_in_full(tmp_path):
    led = ledger_.Ledger(tmp_path)
    base = noise(0, 500)
    for i in range(6):
        led.record(base + 1e-6 * noise(100 + i, 500), f'copy {i}')
    assert led.trials() == pytest.approx(1.0, abs=0.1)
    for i in range(4):
        led.record(noise(200 + i, 500), f'independent {i}')
    assert led.trials() == pytest.approx(5.0, rel=0.15)
    assert ledger_.Ledger(tmp_path / 'empty').trials(extra=3) == 3.0


def test_buffer_flushes_to_files(tmp_path, monkeypatch):
    monkeypatch.setattr(ledger_, 'FLUSH', 3)
    led = ledger_.Ledger(tmp_path)
    for i in range(7):
        led.record(noise(i), f't{i}')
    assert len(list(tmp_path.glob('*.parquet'))) == 2
    led.save()
    assert len(ledger_.Ledger(tmp_path).table()) == 7


def small_panel():
    rng = np.random.default_rng(0)
    rows = [(f'C{j}', pd.Timestamp('2021-01-04', tz='UTC') + pd.Timedelta(days=i), c, c, c, 2e7)
            for j in range(6) for i, c in enumerate(10 * np.exp(np.cumsum(rng.normal(0, 0.04, 400))))]
    return panel.bars(pd.DataFrame(rows, columns=['symbol', 'day', 'high', 'low', 'close', 'volume']), warmup=0)


def test_book_records_every_run_once(tmp_path):
    bars = small_panel()
    p = panel.build(bars)
    led = ledger_.Ledger(tmp_path)
    b = book.Book(p, costs.Flat(5.0), ledger=led)
    w = book.inverse_vol(p, np.sign(p.returns.shift(1)))
    b.run(w, name='sign', params={'L': 1})
    b.run(w, cost_mult=2.0, name='sign at twice cost')
    b.run(w.shift(1), record=False)
    on = pd.Series(np.arange(len(p.returns)) % 3 == 0, index=p.returns.index)
    b.sleeve(w, on, name='sleeve')
    b.by_bar(w)
    t = led.table().set_index('name')
    assert sorted(t.index) == ['sign', 'sign at twice cost', 'sleeve']
    assert t.loc['sign at twice cost', 'kind'] == 'check'
    s = book.Sliced(bars, costs.Flat(5.0), ledger=led).run(lambda q: book.inverse_vol(q, np.sign(q.returns.shift(1))), name='seven')
    assert len(led.table()) == 4 and 'seven' in set(led.table().name)
    np.testing.assert_allclose(led.frame()['seven'].dropna(), s.net.astype('float32'))


def test_score_reads_the_trial_count_from_a_ledger(tmp_path):
    led = ledger_.Ledger(tmp_path)
    for i in range(4):
        led.record(noise(i, 400), f't{i}')
    x = noise(9, 400) + 0.01
    assert gate.score(x, led).probability == gate.score(x, led.trials()).probability


def git(folder, *args):
    subprocess.run(['git', '-c', 'user.email=t@t', '-c', 'user.name=t', *args], cwd=folder, check=True, capture_output=True)


def test_lane_refuses_primary_trials_before_the_spec_is_committed(tmp_path):
    git(tmp_path, 'init', '-q')
    lane = Lane(tmp_path)
    lane.ledger.record(noise(0), 'look around')
    with pytest.raises(SpecNotCommitted):
        lane.ledger.record(noise(1), 'the test', kind='primary')
    lane.spec.write_text('hypothesis\n')
    with pytest.raises(SpecNotCommitted):
        lane.ledger.record(noise(1), 'the test', kind='primary')
    git(tmp_path, 'add', 'spec.md')
    git(tmp_path, 'commit', '-q', '-m', 'spec')
    lane.ledger.record(noise(1), 'the test', kind='primary')
    lane.spec.write_text('hypothesis, amended after the run\n')
    with pytest.raises(SpecNotCommitted):
        lane.ledger.record(noise(2), 'the registered test', kind='registered')
    assert set(lane.ledger.table().name) == {'look around', 'the test'}
