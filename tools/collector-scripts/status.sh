#!/usr/bin/env bash
# Quick health snapshot: running or not, the collector's own recent
# status/warning lines (main.cpp prints these every ~5s — see
# apps/collector/collector.cpp), and a peek at the data dir's manifests
# (D12's gap/resync notes — unfiltered, whatever's actually there).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$SCRIPT_DIR/data"
LOG_FILE="$SCRIPT_DIR/collector.log"
PID_FILE="$SCRIPT_DIR/collector.pid"

if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
    echo "running (pid $(cat "$PID_FILE"))"
else
    echo "not running"
fi

echo
echo "--- last 20 status/warning lines ---"
if [[ -f "$LOG_FILE" ]]; then
    # grep exits 1 (not an error here) if the collector hasn't printed its
    # first status line yet (~5s poll interval) — set -e would otherwise
    # kill the whole script on that, indistinguishable from a real failure.
    grep -E '\[collector\] (status|WARNING)' "$LOG_FILE" | tail -20 || echo "(nothing logged yet)"
else
    echo "(no log yet)"
fi

echo
echo "--- data dir ---"
if [[ -d "$DATA_DIR" ]]; then
    echo "size: $(du -sh "$DATA_DIR" 2>/dev/null | cut -f1)"
    echo "recent manifest entries:"
    find "$DATA_DIR" -name '*.manifest' -exec tail -n 5 {} + 2>/dev/null | tail -20 || true
else
    echo "(no data dir yet)"
fi
