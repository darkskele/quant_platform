import pandas as pd
import pytest

from qp_research import cache as cache_


def ts(s):
    return pd.Timestamp(s, tz='UTC')


class Fake:
    """Daily rows for every symbol across whatever window it is asked for, recording each call."""

    def __init__(self, through=None, absent=()):
        self.calls, self.through, self.absent = [], through, set(absent)

    def __call__(self, symbols, lo, hi):
        self.calls.append((tuple(symbols), lo, hi))
        days = pd.date_range(lo, hi, freq='D', inclusive='left')
        if self.through is not None:
            days = days[days < self.through]
        rows = pd.DataFrame([(s, d, float(i)) for s in symbols if s not in self.absent for i, d in enumerate(days)],
                            columns=['symbol', 't', 'v'])
        return rows, min(hi, self.through) if self.through is not None else hi


@pytest.fixture
def c(tmp_path):
    return cache_.Cache(tmp_path, max_gb=1)


def load(c, fetch, symbols=('A', 'B'), start='2021-01-01', end='2023-01-01', need=None, **kw):
    parts = cache_.years(start, end)
    return c.load('v', 'm', 'd', list(symbols), parts, ts(need or end), fetch, 't', **kw)


def test_partition_spans():
    assert [p.label for p in cache_.years('2021-03-01', '2023-01-01')] == ['year=2021', 'year=2022']
    assert [p.label for p in cache_.months('2021-11-15', '2022-02-01')] == ['year=2021/month=11', 'year=2021/month=12',
                                                                           'year=2022/month=01']
    m = cache_.months('2024-02-01', '2024-03-01')[0]
    assert (m.start, m.end) == (ts('2024-02-01'), ts('2024-03-01'))
    assert cache_.span('2020-01-01', '2021-01-01')[0].label == 'span=2020-01-01_2021-01-01'


def test_second_load_reads_without_fetching(c):
    f = Fake()
    first = load(c, f)
    assert len(f.calls) == 1 and len(first) == 2 * 730
    second = load(c, f)
    assert len(f.calls) == 1
    pd.testing.assert_frame_equal(first.sort_values(['symbol', 't']).reset_index(drop=True),
                                  second.sort_values(['symbol', 't']).reset_index(drop=True))


def test_only_new_symbols_and_spans_are_fetched(c):
    f = Fake()
    load(c, f, symbols=['A'])
    load(c, f, symbols=['A', 'B'])
    assert f.calls[-1][0] == ('B',)
    load(c, f, symbols=['A', 'B'], end='2024-01-01')
    assert f.calls[-1] == (('A', 'B'), ts('2023-01-01'), ts('2024-01-01'))
    assert len(f.calls) == 3


def test_empty_partitions_are_remembered(c):
    f = Fake(absent={'B'})
    out = load(c, f)
    assert set(out.symbol) == {'A'}
    load(c, f)
    assert len(f.calls) == 1


def test_incomplete_partition_refetched_only_when_needed_further(c):
    f = Fake(through=ts('2022-06-01'))
    load(c, f, need='2022-06-01')
    load(c, f, need='2022-06-01')
    assert len(f.calls) == 1
    f.through = None
    out = load(c, f, need='2022-09-01')
    assert len(f.calls) == 2
    assert f.calls[-1][1:] == (ts('2022-01-01'), ts('2023-01-01'))
    assert out.t.max() == ts('2022-12-31')


def test_symbols_fetched_in_chunks(c):
    f = Fake()
    load(c, f, symbols=[f'S{i}' for i in range(120)], end='2022-01-01', chunk=50)
    assert [len(s) for s, _, _ in f.calls] == [50, 50, 20]


def test_info_clear_and_no_temp_files(c, tmp_path):
    load(c, Fake())
    info = c.info()
    assert info.partitions.iloc[0] == 4 and info.rows.iloc[0] == 1460
    assert not list(tmp_path.rglob('*.tmp'))
    assert c.clear(symbol='A') == 2
    assert c.info().partitions.iloc[0] == 2
    assert len(list(tmp_path.rglob('*.parquet'))) == 2


def test_trim_evicts_least_recently_read(c):
    f = Fake()
    load(c, f, symbols=['A'])
    load(c, f, symbols=['B'])
    load(c, f, symbols=['A'])
    one = c.info().gb.iloc[0] / 4
    assert c.trim(max_gb=one * 2.5) == 2
    load(c, f, symbols=['A'])
    assert len(f.calls) == 2
    load(c, f, symbols=['B'])
    assert len(f.calls) == 3


def test_trim_spares_what_a_load_just_read(tmp_path):
    c = cache_.Cache(tmp_path, max_gb=1e-12)
    out = load(c, Fake())
    assert len(out) == 1460
