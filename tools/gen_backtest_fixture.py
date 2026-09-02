#!/usr/bin/env python3
"""Generates tests/apps/backtest/fixtures/.
"""
import os
import random

SYMBOL = "BTCUSDT"
FIRST_DAY = "2024-01-01"
LAST_DAY = "2024-01-07"
FUNDING_MONTH = "2024-01"
OUT_ROOT = os.path.join(os.path.dirname(__file__), "..", "tests", "apps", "backtest", "fixtures")
SEED = 1337

MS_PER_MIN = 60_000
MS_PER_DAY = 1440 * MS_PER_MIN
DAY0_MS = 1_704_067_200_000  # 2024-01-01T00:00:00Z
NUM_DAYS = 7  # FIRST_DAY..LAST_DAY inclusive


def day_str(day_index):
    # FIRST_DAY..LAST_DAY are consecutive; no calendar library needed for
    # a fixed 7-day January window.
    return f"2024-01-{day_index + 1:02d}"


def kline_row(open_time_ms, open_p, high_p, low_p, close_p, volume):
    close_time_ms = open_time_ms + MS_PER_MIN - 1
    return (
        f"{SYMBOL},K,{open_time_ms},{open_p:.2f},{high_p:.2f},{low_p:.2f},"
        f"{close_p:.2f},{volume:.3f},{close_time_ms}"
    )


def mark_row(open_time_ms, open_p, high_p, low_p, close_p):
    close_time_ms = open_time_ms + MS_PER_MIN - 1
    return (
        f"{SYMBOL},M,{open_time_ms},{open_p:.2f},{high_p:.2f},{low_p:.2f},"
        f"{close_p:.2f},0.000,{close_time_ms}"
    )


def funding_row(calc_time_ms, rate):
    return f"{SYMBOL},F,{calc_time_ms},8,{rate:.6f}"


def write(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    rng = random.Random(SEED)

    futures_price = 50_000.0
    spot_basis = 1.0  # spot = futures * spot_basis, drifts slowly

    for day in range(NUM_DAYS):
        futures_kline_lines = []
        futures_mark_lines = []
        spot_kline_lines = []

        for minute in range(1440):
            open_time_ms = DAY0_MS + day * MS_PER_DAY + minute * MS_PER_MIN

            # Futures: small random-walk step per minute.
            step = rng.gauss(0.0, 15.0)
            open_p = futures_price
            futures_price = max(1.0, futures_price + step)
            close_p = futures_price
            high_p = max(open_p, close_p) + abs(rng.gauss(0.0, 3.0))
            low_p = min(open_p, close_p) - abs(rng.gauss(0.0, 3.0))
            volume = abs(rng.gauss(2.0, 0.5))
            futures_kline_lines.append(kline_row(open_time_ms, open_p, high_p, low_p, close_p, volume))

            # Mark price: tracks the futures close with tiny independent noise.
            mark_close = close_p + rng.gauss(0.0, 1.0)
            futures_mark_lines.append(
                mark_row(open_time_ms, open_p, max(open_p, mark_close), min(open_p, mark_close),
                        mark_close))

            # Spot: futures price scaled by a slowly drifting basis, its
            # own independent small noise on top.
            spot_basis += rng.gauss(0.0, 0.00002)
            spot_basis = min(max(spot_basis, 0.995), 1.005)
            spot_close = futures_price * spot_basis + rng.gauss(0.0, 2.0)
            spot_open = open_p * spot_basis
            spot_high = max(spot_open, spot_close) + abs(rng.gauss(0.0, 2.0))
            spot_low = min(spot_open, spot_close) - abs(rng.gauss(0.0, 2.0))
            spot_volume = abs(rng.gauss(3.0, 0.7))
            spot_kline_lines.append(
                kline_row(open_time_ms, spot_open, spot_high, spot_low, spot_close, spot_volume))

        d = day_str(day)
        write(os.path.join(OUT_ROOT, SYMBOL, "futures", "klines", f"{SYMBOL}-1m-{d}.csv"),
              futures_kline_lines)
        write(os.path.join(OUT_ROOT, SYMBOL, "futures", "markprice", f"{SYMBOL}-1m-{d}.csv"),
              futures_mark_lines)
        write(os.path.join(OUT_ROOT, SYMBOL, "spot", "klines", f"{SYMBOL}-1m-{d}.csv"),
              spot_kline_lines)

    # Funding: every 8h (3/day = 21 events over 7 days). Deliberately
    # oscillates above the 0.0001 entry threshold, between the two
    # thresholds, and at/below the 0.0 exit threshold, in a fixed cycle —
    # so the run actually exercises FundingCarryStrategy's enter/hold/exit
    # branches multiple times, not just "enters once and sits there".
    funding_lines = []
    cycle = [0.0003, 0.00005, -0.0001, 0.0002, 0.0, 0.00015, -0.0002]
    for i in range(NUM_DAYS * 3):
        calc_time_ms = DAY0_MS + i * (8 * 60 * MS_PER_MIN)
        rate = cycle[i % len(cycle)]
        funding_lines.append(funding_row(calc_time_ms, rate))
    write(os.path.join(OUT_ROOT, SYMBOL, "futures", "funding", f"{SYMBOL}-fundingRate-{FUNDING_MONTH}.csv"),
          funding_lines)

    print(f"Wrote fixture data under {os.path.abspath(OUT_ROOT)}")


if __name__ == "__main__":
    main()
