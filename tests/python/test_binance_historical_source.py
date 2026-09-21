"""End to end proof of the Binance historical source through its python binding.

Every dataset added after klines and funding is pulled through the source and
checked row by row against the same file fetched and parsed here with nothing
but the standard library. Two independent readings of the bucket have to agree.
"""
from __future__ import annotations

import csv
import io
import math
import threading
import urllib.request
import zipfile
from datetime import datetime, timezone

from qp_module import load

qpb = load()

EndpointKind = qpb.EndpointKind
EventKind = qpb.EventKind
SPOT = int(qpb.BinanceMarket.Spot)
USDM = int(qpb.BinanceMarket.UsdM)
COINM = int(qpb.BinanceMarket.CoinM)

NS = 1_000_000_000
# 2025-06-02 00:00:00 UTC, and the last second of that day and of 2025-06-04.
DAY_FROM = 1748822400 * NS
DAY_TO = 1748908799 * NS
THREE_DAYS_TO = 1749081599 * NS

HOST = "https://data.binance.vision/data"


# ---------------------------------------------------------------------------
# Oracle. Its own urls, its own csv reading, its own stamp rules.
# ---------------------------------------------------------------------------


def fetch_rows(path):
    with urllib.request.urlopen(f"{HOST}/{path}", timeout=120) as response:
        archive = zipfile.ZipFile(io.BytesIO(response.read()))
    text = archive.read(archive.namelist()[0]).decode()
    return [r for r in csv.reader(io.StringIO(text)) if r and r[0][:1].isdigit()]


def datetime_ns(text):
    stamp = datetime.strptime(text, "%Y-%m-%d %H:%M:%S").replace(tzinfo=timezone.utc)
    return int(stamp.timestamp()) * NS


def epoch_ns(text):
    raw = int(text)
    return raw * 1_000 if len(text) >= 16 else raw * 1_000_000


def close(a, b):
    if math.isnan(a) or math.isnan(b):
        return math.isnan(a) and math.isnan(b)
    return math.isclose(a, b, rel_tol=1e-12, abs_tol=0.0)


def opt(text):
    return float(text) if text else math.nan


# ---------------------------------------------------------------------------
# Source side.
# ---------------------------------------------------------------------------


def make_source(pairs, streams, to_ns=DAY_TO, workers=8):
    builder = qpb.SubscriptionBuilder()
    for market, symbol in pairs:
        builder.add(qpb.ExchangeId.Binance, market, symbol)
    subscription = builder.build()

    pool = qpb.FetchPoolConfig()
    pool.workers = workers
    config = qpb.BinanceHistoricalConfig(
        [qpb.StreamSpec(kind) for kind in streams], qpb.Cadence.Daily, DAY_FROM, to_ns, pool
    )
    source = qpb.BinanceHistoricalSource(subscription, config)
    source.plan()
    names = {}
    for market, symbol in pairs:
        instrument = subscription.resolve(qpb.ExchangeId.Binance, market, symbol)
        names[(instrument.market, instrument.symbol)] = symbol
    return source, names


def assert_clean(source):
    stats = source.fetch_stats()
    assert stats.completed_failed == 0, f"{stats.completed_failed} fetches failed"
    for report in source.reports():
        s = report.stats
        assert s.files_failed == 0, f"{report.symbol} lost files"
        assert s.rows_rejected == 0, f"{report.symbol} rejected {s.rows_rejected} rows"
        assert s.backwards_stamps == 0, f"{report.symbol} went backwards"
        assert s.sequence_gaps == 0, f"{report.symbol} has {s.sequence_gaps} gaps in its ids"


# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------


def test_metrics_match_the_file_on_both_futures_markets():
    pairs = [(USDM, "BTCUSDT"), (COINM, "BTCUSD_PERP")]
    source, names = make_source(pairs, [EndpointKind.Metrics])

    got = {}
    for event in source:
        assert event.base.kind == EventKind.OpenInterest
        symbol = names[(event.base.market, event.base.symbol)]
        got[(symbol, event.base.ts)] = event.payload
    assert_clean(source)

    expected = 0
    for market_path, symbol in (("futures/um", "BTCUSDT"), ("futures/cm", "BTCUSD_PERP")):
        rows = fetch_rows(f"{market_path}/daily/metrics/{symbol}/{symbol}-metrics-2025-06-02.zip")
        expected += len(rows)
        for row in rows:
            p = got[(symbol, datetime_ns(row[0]))]
            assert close(p.open_interest, float(row[2]))
            assert close(p.open_interest_value, float(row[3]))
            assert close(p.toptrader_account_ratio, opt(row[4]))
            assert close(p.toptrader_position_ratio, opt(row[5]))
            assert close(p.account_long_short_ratio, opt(row[6]))
            assert close(p.taker_long_short_volume_ratio, opt(row[7]))
    assert len(got) == expected


def test_coin_m_metrics_ratios_arrive_as_nan_not_as_rejects():
    source, _ = make_source([(COINM, "BTCUSD_PERP")], [EndpointKind.Metrics])
    events = list(source)
    assert events, "no coin-m metrics arrived"
    assert all(math.isnan(e.payload.toptrader_account_ratio) for e in events)
    assert all(not math.isnan(e.payload.taker_long_short_volume_ratio) for e in events)
    assert_clean(source)


def test_early_metrics_publish_every_row_twice_and_the_source_counts_it():
    day = 1600128000 * NS  # 2020-09-15
    builder = qpb.SubscriptionBuilder()
    builder.add(qpb.ExchangeId.Binance, USDM, "BTCUSDT")
    config = qpb.BinanceHistoricalConfig(
        [qpb.StreamSpec(EndpointKind.Metrics)], qpb.Cadence.Daily, day, day + 86399 * NS
    )
    source = qpb.BinanceHistoricalSource(builder.build(), config)
    source.plan()
    events = list(source)

    [report] = source.reports()
    assert report.stats.rows_parsed == 576 == len(events)
    assert report.stats.repeated_stamps == 288
    assert len({e.base.ts for e in events}) == 288


def test_book_depth_samples_match_the_file_on_both_futures_markets():
    pairs = [(USDM, "BTCUSDT"), (COINM, "BTCUSD_PERP")]
    source, names = make_source(pairs, [EndpointKind.BookDepth])

    got = {}
    for event in source:
        assert event.base.kind == EventKind.BookDepth
        symbol = names[(event.base.market, event.base.symbol)]
        got[(symbol, event.base.ts)] = event.payload.bands
    assert_clean(source)

    samples = {}
    for market_path, symbol in (("futures/um", "BTCUSDT"), ("futures/cm", "BTCUSD_PERP")):
        path = f"{market_path}/daily/bookDepth/{symbol}/{symbol}-bookDepth-2025-06-02.zip"
        for row in fetch_rows(path):
            key = (symbol, datetime_ns(row[0]))
            samples.setdefault(key, {})[int(row[1])] = (float(row[2]), float(row[3]))

    assert len(got) == len(samples)
    for key, bands in samples.items():
        assert len(bands) == 10
        mine = got[key]
        for k in range(5):
            assert close(mine.bids[k].depth, bands[-(k + 1)][0])
            assert close(mine.bids[k].notional, bands[-(k + 1)][1])
            assert close(mine.asks[k].depth, bands[k + 1][0])
            assert close(mine.asks[k].notional, bands[k + 1][1])


def test_agg_trades_match_the_file_and_carry_their_ids():
    source, _ = make_source([(COINM, "BTCUSD_PERP")], [EndpointKind.AggTrades])
    got = [(e.base.ts, e.payload) for e in source]
    assert_clean(source)

    rows = fetch_rows("futures/cm/daily/aggTrades/BTCUSD_PERP/BTCUSD_PERP-aggTrades-2025-06-02.zip")
    assert len(got) == len(rows)
    for (ts, trade), row in zip(got, rows):
        assert trade.id == int(row[0])
        assert close(trade.price, float(row[1]))
        assert close(trade.qty, float(row[2]))
        assert trade.first_trade_id == int(row[3])
        assert trade.last_trade_id == int(row[4])
        assert ts == epoch_ns(row[5])
        assert (trade.side == qpb.Side.Sell) == (row[6].lower() == "true")


def test_spot_agg_trades_match_the_headerless_microsecond_file():
    source, _ = make_source([(SPOT, "BTCUSDT")], [EndpointKind.AggTrades])
    got = [(e.base.ts, e.payload) for e in source]
    assert_clean(source)

    rows = fetch_rows("spot/daily/aggTrades/BTCUSDT/BTCUSDT-aggTrades-2025-06-02.zip")
    assert len(rows[0]) == 8, "spot carries is_best_match"
    assert len(rows[0][5]) == 16, "spot is microseconds from 2025"
    assert len(got) == len(rows)
    for (ts, trade), row in zip(got, rows):
        assert trade.id == int(row[0])
        assert close(trade.price, float(row[1]))
        assert close(trade.qty, float(row[2]))
        assert trade.first_trade_id == int(row[3])
        assert trade.last_trade_id == int(row[4])
        assert ts == epoch_ns(row[5])
        assert (trade.side == qpb.Side.Sell) == (row[6] == "True")


def test_agg_trade_ids_run_on_across_day_boundaries():
    source, _ = make_source([(COINM, "BTCUSD_PERP")], [EndpointKind.AggTrades], THREE_DAYS_TO)
    ids = [e.payload.id for e in source]
    assert_clean(source)

    [report] = source.reports()
    assert report.stats.files_read == 3
    assert ids == list(range(ids[0], ids[0] + len(ids))), "ids are not one unbroken run"


def test_every_new_dataset_merges_into_one_ascending_stream():
    kinds = [EndpointKind.Metrics, EndpointKind.BookDepth, EndpointKind.AggTrades]
    source, _ = make_source([(COINM, "BTCUSD_PERP")], kinds)

    counts = {EventKind.OpenInterest: 0, EventKind.BookDepth: 0, EventKind.Trade: 0}
    previous = 0
    for event in source:
        assert event.base.ts >= previous, "the merge went backwards"
        previous = event.base.ts
        counts[event.base.kind] += 1
    assert_clean(source)

    by_kind = {r.kind: r.stats for r in source.reports()}
    assert counts[EventKind.OpenInterest] == by_kind[EndpointKind.Metrics].rows_parsed
    assert counts[EventKind.BookDepth] * 10 == by_kind[EndpointKind.BookDepth].rows_parsed
    assert counts[EventKind.Trade] == by_kind[EndpointKind.AggTrades].rows_parsed
    assert all(counts.values()), counts


def test_next_returns_none_once_exhausted():
    source, _ = make_source([(COINM, "BTCUSD_PERP")], [EndpointKind.Metrics])
    while source.next() is not None:
        pass
    assert source.next() is None
    assert list(source) == []


def test_waiting_on_the_network_releases_the_gil():
    source, _ = make_source([(COINM, "BTCUSD_PERP")], [EndpointKind.AggTrades], THREE_DAYS_TO)
    ticks = [0]
    stop = threading.Event()

    def spin():
        while not stop.is_set():
            ticks[0] += 1

    thread = threading.Thread(target=spin)
    thread.start()
    try:
        for _ in source:
            pass
    finally:
        stop.set()
        thread.join()
    assert ticks[0] > 0, "a python thread never ran while the source waited"
