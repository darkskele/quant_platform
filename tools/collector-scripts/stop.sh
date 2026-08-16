#!/usr/bin/env bash
# Clean SIGTERM shutdown (not kill -9) — lets the collector drain its
# queues and close FileRecorder's partitions properly, same shutdown path
# main.cpp installs the signal handler for. Waits up to 30s before giving
# up and reporting failure rather than declaring success prematurely.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_FILE="$SCRIPT_DIR/collector.pid"

if [[ ! -f "$PID_FILE" ]]; then
    echo "not running (no pidfile)" >&2
    exit 1
fi

PID="$(cat "$PID_FILE")"
if ! kill -0 "$PID" 2>/dev/null; then
    echo "stale pidfile (pid $PID not running) — removing" >&2
    rm -f "$PID_FILE"
    exit 1
fi

kill -TERM "$PID"
echo "sent SIGTERM to pid $PID, waiting for clean shutdown..."
for _ in $(seq 1 30); do
    if ! kill -0 "$PID" 2>/dev/null; then
        rm -f "$PID_FILE"
        echo "stopped"
        exit 0
    fi
    sleep 1
done

echo "still running after 30s — check $SCRIPT_DIR/collector.log" >&2
exit 1
