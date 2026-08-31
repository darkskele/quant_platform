#!/usr/bin/env bash
# Configures + builds qp_backtest against one Source/Sink/Matcher/Risk/
# Strategy combo (see include/apps/backtest/config/*.hpp for what each name
# resolves to) and one pre-decided (symbol, dataset) pair (see
# config/data_layout.hpp) — nothing about either is a runtime flag to
# qp_backtest itself (D5x): this script's flags are the only "config" for
# them. Each axis is an independent CMake cache var, so changing one alone
# doesn't force a full reconfigure of the others.
#
# Usage: tools/build_backtest.sh [--source NAME] [--sink NAME]
#          [--matcher NAME] [--risk NAME] [--strategy NAME]
#          [--data-dir DIR] [--symbol SYM] [--first-day DATE] [--last-day DATE]
# All flags optional; defaults match the one real combo + BTCUSDT/2024-01-01
# that exists today. Data for the (symbol, day-range) pair must already be
# under --data-dir in tools/fetch_backtest_data.sh's layout — this script
# doesn't fetch it.
set -euo pipefail

SOURCE="csv_binhist"
SINK="fanout"
MATCHER="last_trade"
RISK="basic"
STRATEGY="funding_carry"
DATA_DIR="data/binance_historical"
SYMBOL="BTCUSDT"
FIRST_DAY="2024-01-01"
LAST_DAY="2024-01-01"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source) SOURCE="$2"; shift 2 ;;
        --sink) SINK="$2"; shift 2 ;;
        --matcher) MATCHER="$2"; shift 2 ;;
        --risk) RISK="$2"; shift 2 ;;
        --strategy) STRATEGY="$2"; shift 2 ;;
        --data-dir) DATA_DIR="$2"; shift 2 ;;
        --symbol) SYMBOL="$2"; shift 2 ;;
        --first-day) FIRST_DAY="$2"; shift 2 ;;
        --last-day) LAST_DAY="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 1 ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

cmake --preset release \
    -DQP_BACKTEST_SOURCE="$SOURCE" \
    -DQP_BACKTEST_SINK="$SINK" \
    -DQP_BACKTEST_MATCHER="$MATCHER" \
    -DQP_BACKTEST_RISK="$RISK" \
    -DQP_BACKTEST_STRATEGY="$STRATEGY" \
    -DQP_BACKTEST_DATA_DIR="$DATA_DIR" \
    -DQP_BACKTEST_SYMBOL="$SYMBOL" \
    -DQP_BACKTEST_FIRST_DAY="$FIRST_DAY" \
    -DQP_BACKTEST_LAST_DAY="$LAST_DAY"

cmake --build build/release --target qp_backtest -j
