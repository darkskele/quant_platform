#!/usr/bin/env python3
"""Profiles a backtest-scale Binance historical run through the Python bindings.

Two markets, two symbols, five years of hourly bars and funding, fetched from
the live bucket. Reports every phase separately, then the per-event
distribution from both sides of the boundary, so a slow run can be attributed
to the network, the parse, the engine or the Python callback rather than
guessed at.
"""
from __future__ import annotations

import harness

YEARS = 5
WORKERS = 32


def main():
    overhead = harness.clock_overhead_ns()
    backtest, profile, phases, results, end_day = harness.run(YEARS, WORKERS, "strategy")
    harness.report(backtest, profile, phases, results, YEARS, end_day, overhead, WORKERS)


if __name__ == "__main__":
    main()
