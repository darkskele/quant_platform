"""End to end proof of the Binance historical backtest app against the live bucket.

Fetches real monthly files, runs the engine with Python strategy and risk
callbacks, and checks what came back against what the bucket says should be
there. Nothing is mocked and nothing is generated.
"""
from __future__ import annotations

import threading

from qp_module import load

qpb = load()

EventKind = qpb.EventKind
Intent, Order, Side = qpb.Intent, qpb.Order, qpb.Side
RiskDecision, RiskOutcome = qpb.RiskDecision, qpb.RiskOutcome

# 2025-01-01 and 2025-02-01 as nanoseconds. One monthly file per stream, which
# is the smallest window that still crosses a file boundary on the read side.
FROM_NS = 1735689600000000000
TO_NS = 1738368000000000000

MARKET = int(qpb.BinanceMarket.UsdM)
SYMBOLS = ["BTCUSDT", "ETHUSDT"]

# Hourly bars, so two symbols collide on every timestamp. Funding lands on the
# eight hour boundaries, so the two streams interleave rather than concatenate.
STREAMS = [
    qpb.StreamSpec(qpb.EndpointKind.Klines, "1h"),
    qpb.StreamSpec(qpb.EndpointKind.FundingRate),
]

EXPECTED_STREAMS = len(SYMBOLS) * len(STREAMS)


def build_subscription():
    builder = qpb.SubscriptionBuilder()
    for symbol in SYMBOLS:
        builder.add(qpb.ExchangeId.Binance, MARKET, symbol)
    return builder.build()


def make_backtest(workers=8):
    subscription = build_subscription()
    pool = qpb.FetchPoolConfig()
    pool.workers = workers
    config = qpb.BinanceHistoricalConfig(
        STREAMS, qpb.Cadence.Monthly, FROM_NS, TO_NS, pool
    )
    return qpb.PythonBacktest(subscription, config), subscription


def slot(subscription, symbol):
    """The symbol's slot id, which is what an event carries and the book keys on."""
    instrument = subscription.resolve(qpb.ExchangeId.Binance, MARKET, symbol)
    assert instrument is not None, f"{symbol} is not in the subscription"
    return instrument.symbol


def approve_everything(counter):
    def check(intent):
        counter[0] += 1
        return RiskDecision(
            RiskOutcome.Approved,
            Order(
                counter[0],
                intent.exchange,
                intent.market,
                intent.symbol,
                Side.Buy,
                intent.target_position,
            ),
        )

    return check


def test_plan_discovers_every_stream():
    bt, _ = make_backtest()
    bt.plan()

    assert bt.stream_count() == EXPECTED_STREAMS, f"stream_count {bt.stream_count()}"
    reports = bt.reports()
    assert len(reports) == EXPECTED_STREAMS, f"{len(reports)} reports"
    for report in reports:
        assert report.stats.files_planned > 0, f"{report.symbol} {report.kind} planned nothing"


def test_every_planned_file_is_fetched_and_read():
    bt, _ = make_backtest()
    bt.plan()
    bt.run()

    for report in bt.reports():
        name = f"{report.symbol} {report.kind} {report.interval}"
        stats = report.stats
        assert stats.files_failed == 0, f"{name} failed {stats.files_failed}"
        assert stats.files_read == stats.files_planned, (
            f"{name} read {stats.files_read} of {stats.files_planned}"
        )

    stats = bt.fetch_stats()
    assert stats.completed_failed == 0, f"{stats.completed_failed} fetches failed"
    assert stats.not_found == 0, f"{stats.not_found} files missing from the bucket"
    assert stats.completed_ok == stats.submitted, "submitted and completed disagree"
    # Inflating is the whole point of the zip path, so equality means the
    # archive layer silently passed the bytes through.
    assert stats.bytes_inflated > stats.bytes_fetched > 0, (
        f"inflated {stats.bytes_inflated} vs fetched {stats.bytes_fetched}"
    )


def test_no_rows_are_lost_between_the_file_and_the_engine():
    bt, _ = make_backtest()
    bt.plan()
    bt.run()

    for report in bt.reports():
        name = f"{report.symbol} {report.kind} {report.interval}"
        stats = report.stats
        assert stats.rows_parsed > 0, f"{name} parsed nothing"
        assert stats.rows_rejected == 0, f"{name} rejected {stats.rows_rejected}"
        assert stats.backwards_stamps == 0, f"{name} had {stats.backwards_stamps} backwards"
        # A row count can only be predicted from an interval, and funding has
        # none, so those streams report zero expected rather than a shortfall.
        if stats.rows_expected:
            assert stats.rows_parsed == stats.rows_expected, (
                f"{name} parsed {stats.rows_parsed} of {stats.rows_expected}"
            )


def test_events_arrive_in_non_decreasing_timestamp_order():
    bt, _ = make_backtest()
    bt.plan()

    seen = []
    bt.set_on_event(lambda ev: seen.append(ev.base.ts))
    bt.run()

    assert seen, "no events reached the strategy"
    for i in range(1, len(seen)):
        assert seen[i] >= seen[i - 1], f"event {i} went backwards, {seen[i]} after {seen[i - 1]}"

    parsed = sum(r.stats.rows_parsed for r in bt.reports())
    assert len(seen) == parsed, f"strategy saw {len(seen)} of {parsed} parsed rows"


def test_the_event_kind_filter_keeps_the_others_out():
    bt, _ = make_backtest()
    bt.plan()

    kinds = set()
    bt.set_on_event(lambda ev: kinds.add(ev.base.kind))
    bt.set_event_kinds([EventKind.Funding])
    bt.run()

    assert kinds == {EventKind.Funding}, f"saw {kinds} with only Funding subscribed"


def test_a_strategy_that_never_trades_leaves_the_book_untouched():
    bt, subscription = make_backtest()
    bt.plan()
    bt.set_on_event(lambda ev: None)
    bt.run()

    results = bt.results()
    assert results.final_cash == results.final_equity, "equity moved without a position"
    assert all(f == 0.0 for f in results.fees), f"fees charged with no trade, {results.fees}"
    for symbol in SYMBOLS:
        held = bt.position(int(qpb.ExchangeId.Binance), MARKET, slot(subscription, symbol))
        assert held == 0.0, f"{symbol} holds {held} after a flat run"


def test_a_trading_strategy_moves_the_position_and_pays_fees():
    bt, subscription = make_backtest()
    bt.plan()

    btc = slot(subscription, "BTCUSDT")
    counter = [0]

    def on_event(ev):
        if ev.base.kind == EventKind.Funding and ev.base.symbol == btc:
            return [Intent(ev.base.exchange, ev.base.market, ev.base.symbol, 1.0)]
        return None

    bt.set_on_event(on_event)
    bt.set_check(approve_everything(counter))
    bt.run()

    assert counter[0] > 0, "risk gate never saw an intent"
    results = bt.results()
    assert any(f > 0.0 for f in results.fees), f"no fees after {counter[0]} orders"
    assert results.final_cash != results.final_equity, (
        "cash and equity identical while holding a position"
    )
    assert len(results.equity_series) > 0, "no equity samples recorded"


def test_two_identical_runs_agree_exactly():
    def run_once():
        bt, subscription = make_backtest()
        bt.plan()
        btc = slot(subscription, "BTCUSDT")
        seen = []
        counter = [0]

        def on_event(ev):
            seen.append((ev.base.ts, ev.base.market, ev.base.symbol, int(ev.base.kind)))
            if ev.base.kind == EventKind.Funding and ev.base.symbol == btc:
                return [Intent(ev.base.exchange, ev.base.market, ev.base.symbol, 1.0)]
            return None

        bt.set_on_event(on_event)
        bt.set_check(approve_everything(counter))
        bt.run()
        results = bt.results()
        series = [(p.ts, p.equity) for p in results.equity_series]
        return seen, results.final_equity, results.final_cash, series

    first = run_once()
    second = run_once()

    assert first[0] == second[0], "event streams differ between runs"
    assert first[1] == second[1], f"final equity {first[1]} then {second[1]}"
    assert first[2] == second[2], f"final cash {first[2]} then {second[2]}"
    assert first[3] == second[3], "equity series differ between runs"


def test_a_raising_callback_surfaces_instead_of_hanging():
    bt, _ = make_backtest()
    bt.plan()

    def on_event(ev):
        raise ValueError("strategy exploded")

    bt.set_on_event(on_event)

    outcome = {}

    def target():
        try:
            bt.run()
            outcome["result"] = "returned"
        except BaseException as exc:  # noqa: BLE001
            outcome["result"] = type(exc).__name__

    runner = threading.Thread(target=target, daemon=True)
    runner.start()
    runner.join(timeout=120)

    assert not runner.is_alive(), "run() hung on a raising callback"
    assert "result" in outcome, "run() produced no outcome"



def test_the_timer_fires_on_its_configured_period():
    bt, _ = make_backtest()
    bt.plan()

    ticks = []
    bt.set_on_event(lambda ev: None)
    bt.set_on_timer(lambda now: ticks.append(now))
    bt.set_timer_period(60 * 60 * 1_000_000_000)
    bt.run()

    assert ticks, "timer never fired"
    for i in range(1, len(ticks)):
        assert ticks[i] > ticks[i - 1], f"timer went backwards at {i}"
