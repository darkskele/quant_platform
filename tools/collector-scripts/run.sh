#!/usr/bin/env bash
# Starts qp_collector in the background: stdout/stderr -> collector.log,
# pid -> collector.pid (so stop.sh/status.sh can find it later). Data lands
# in ./data next to this script, inside the packaged bundle — self-
# contained, no paths outside the bundle directory assumed.
#
# usage: ./run.sh SYMBOL [SYMBOL...] [--duration SECONDS] [--testnet]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$SCRIPT_DIR/data"
LOG_FILE="$SCRIPT_DIR/collector.log"
PID_FILE="$SCRIPT_DIR/collector.pid"

if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
    echo "already running (pid $(cat "$PID_FILE"))" >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    echo "usage: $0 SYMBOL [SYMBOL...] [--duration SECONDS] [--testnet]" >&2
    exit 1
fi

mkdir -p "$DATA_DIR"
nohup "$SCRIPT_DIR/qp_collector" "$@" --data-dir "$DATA_DIR" >>"$LOG_FILE" 2>&1 &
disown
echo $! >"$PID_FILE"
echo "started (pid $(cat "$PID_FILE")) -> data: $DATA_DIR, log: $LOG_FILE"
