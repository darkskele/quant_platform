#!/usr/bin/env bash
# Builds qp_collector (release) and stages it into a self-contained zipped
# bundle — binary + run/stop/status scripts — ready to copy to a VM and run
# standalone, no repo checkout needed there. Run from anywhere; resolves
# paths off its own location.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

cmake --preset release >/dev/null
cmake --build build/release --target qp_collector -j

STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGE_DIR"' EXIT

BUNDLE_NAME="qp_collector-$(date -u +%Y%m%dT%H%M%SZ)"
BUNDLE_DIR="$STAGE_DIR/$BUNDLE_NAME"
mkdir -p "$BUNDLE_DIR"

cp build/release/apps/collector/qp_collector "$BUNDLE_DIR/"
cp tools/collector-scripts/run.sh "$BUNDLE_DIR/"
cp tools/collector-scripts/stop.sh "$BUNDLE_DIR/"
cp tools/collector-scripts/status.sh "$BUNDLE_DIR/"
chmod +x "$BUNDLE_DIR"/*.sh "$BUNDLE_DIR/qp_collector"

OUT_DIR="$REPO_ROOT/dist"
mkdir -p "$OUT_DIR"
OUT_TARBALL="$OUT_DIR/$BUNDLE_NAME.tar.gz"
# tar.gz, not zip: this deploys to a Linux VM (docs/environment.md), tar is
# always present there with no new dependency, zip isn't installed on this
# box at all.
(cd "$STAGE_DIR" && tar -czf "$OUT_TARBALL" "$BUNDLE_NAME")

echo "packaged: $OUT_TARBALL"
