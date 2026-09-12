#!/usr/bin/env bash
# Create a git worktree as a sibling dir with the shared data store linked in.
# Each worktree is an isolated branch checkout for a concurrent chat, sharing
# one .git and one ~/quant-data. Build dirs stay per-worktree.
#
# Usage: tools/qp-worktree.sh <branch> [base]
#   <branch>  new branch, worktree lands at ../quant-platform-<branch>
#   [base]    branch to fork from, default main
set -euo pipefail

branch="$1"
base="${2:-main}"
repo="$(git rev-parse --show-toplevel)"
wt="$(dirname "$repo")/quant-platform-${branch//\//-}"
data="$HOME/quant-data"

[ -d "$data" ] || { echo "no shared data store at $data" >&2; exit 1; }
git -C "$repo" worktree add -b "$branch" "$wt" "$base"
ln -s "$data" "$wt/data"

echo "worktree ready: $wt"
echo "  branch $branch off $base, data -> $data"
echo "  first build: cmake --preset release -DQP_BACKTEST_MATCHER=cost_aware && cmake --build --preset release"
