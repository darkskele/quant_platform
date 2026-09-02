#!/usr/bin/env bash
# Downloads data.binance.vision's free historical dumps for the fixed
# 10-symbol universe (must match include/data_source/source/venue/binance/
# binance_historical/bin_hist_symbol_table.hpp's detail::kSymbols — update
# both if that list ever changes) and re-tags every line
# "SYMBOL,KIND,<original CSV fields>" (KIND: K=kline, M=markPriceKline,
# F=fundingRate) into the directory layout
# include/apps/backtest/config/data_layout.hpp reads from. The two must
# agree — this script's layout comments and that header's are the same
# text on purpose.
#
# Verified against real data.binance.vision responses (2026-08-31), not
# assumed (D13's own lesson: spot and futures differ even where you'd
# expect them not to):
#   - futures klines/markPriceKlines: daily dumps, HAVE a header row.
#   - futures fundingRate: MONTHLY-only (the daily path 404s) — HAS a
#     header row.
#   - spot klines: daily dumps, NO header row.
#   - spot has neither markPriceKline nor fundingRate (no perpetual, no
#     official mark) — nothing to fetch for those on the spot leg.
#
# Usage: tools/fetch_backtest_data.sh [--data-dir DIR] [--symbols A,B,...]
#          [--first-day YYYY-MM-DD] [--last-day YYYY-MM-DD]
# Re-running is safe and cheap: already-tagged output files are skipped,
# not re-downloaded (historical dumps never change once published).
set -euo pipefail

DATA_DIR="data/binance_historical"
SYMBOLS="BTCUSDT,ETHUSDT,SOLUSDT,BNBUSDT,XRPUSDT,DOGEUSDT,ADAUSDT,LINKUSDT,AVAXUSDT,LTCUSDT"
FIRST_DAY="2022-01-01"
LAST_DAY="2024-12-31"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --data-dir) DATA_DIR="$2"; shift 2 ;;
        --symbols) SYMBOLS="$2"; shift 2 ;;
        --first-day) FIRST_DAY="$2"; shift 2 ;;
        --last-day) LAST_DAY="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 1 ;;
    esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

# Prefers unzip (faster); every dev box this targets (WSL Ubuntu, the VM)
# has python3, so that's the guaranteed fallback, not a second real
# dependency to install.
extract() {
    local zip="$1" dest="$2"
    if command -v unzip >/dev/null 2>&1; then
        unzip -o -q "$zip" -d "$dest"
    else
        python3 -m zipfile -e "$zip" "$dest"
    fi
}

# Downloads $1 into a temp zip, extracts it, strips the header row if
# $4=has_header, prepends "$3,$5," to every remaining line, writes the
# result to $2. Skips entirely if $2 already exists (dumps are immutable
# once published) or the remote 404s (e.g. a symbol with no futures market
# on some exchange, or a day before listing).
fetch_and_tag() {
    local url="$1" out_path="$2" symbol="$3" has_header="$4" kind="$5"
    if [[ -f "$out_path" ]]; then
        return
    fi
    local zip="$TMP_DIR/dl.zip"
    local status
    status="$(curl -s -o "$zip" -w '%{http_code}' "$url")"
    if [[ "$status" != "200" ]]; then
        echo "skip (HTTP $status): $url" >&2
        rm -f "$zip"
        return
    fi
    local extract_dir="$TMP_DIR/extracted"
    rm -rf "$extract_dir"
    mkdir -p "$extract_dir"
    extract "$zip" "$extract_dir"
    local csv
    csv="$(find "$extract_dir" -name '*.csv' | head -n1)"
    mkdir -p "$(dirname "$out_path")"
    local skip=0
    [[ "$has_header" == "1" ]] && skip=1
    tail -n "+$((skip + 1))" "$csv" | sed "s/^/${symbol},${kind},/" > "$out_path"
    rm -f "$zip"
    rm -rf "$extract_dir"
}

IFS=',' read -r -a symbol_list <<< "$SYMBOLS"

for symbol in "${symbol_list[@]}"; do
    echo "=== $symbol ==="
    day="$FIRST_DAY"
    while [[ "$day" < "$LAST_DAY" || "$day" == "$LAST_DAY" ]]; do
        fetch_and_tag \
            "https://data.binance.vision/data/futures/um/daily/klines/${symbol}/1m/${symbol}-1m-${day}.zip" \
            "$DATA_DIR/${symbol}/futures/klines/${symbol}-1m-${day}.csv" "$symbol" 1 K
        fetch_and_tag \
            "https://data.binance.vision/data/futures/um/daily/markPriceKlines/${symbol}/1m/${symbol}-1m-${day}.zip" \
            "$DATA_DIR/${symbol}/futures/markprice/${symbol}-1m-${day}.csv" "$symbol" 1 M
        fetch_and_tag \
            "https://data.binance.vision/data/spot/daily/klines/${symbol}/1m/${symbol}-1m-${day}.zip" \
            "$DATA_DIR/${symbol}/spot/klines/${symbol}-1m-${day}.csv" "$symbol" 0 K
        day="$(date -u -d "$day + 1 day" +%Y-%m-%d)"
    done

    # fundingRate is monthly-only — one fetch per distinct YYYY-MM touched.
    month="$(date -u -d "$FIRST_DAY" +%Y-%m)"
    last_month="$(date -u -d "$LAST_DAY" +%Y-%m)"
    while [[ "$month" < "$last_month" || "$month" == "$last_month" ]]; do
        fetch_and_tag \
            "https://data.binance.vision/data/futures/um/monthly/fundingRate/${symbol}/${symbol}-fundingRate-${month}.zip" \
            "$DATA_DIR/${symbol}/futures/funding/${symbol}-fundingRate-${month}.csv" "$symbol" 1 F
        month="$(date -u -d "${month}-01 + 1 month" +%Y-%m)"
    done
done

echo "done: $DATA_DIR"
