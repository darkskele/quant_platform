#!/usr/bin/env bash
# Create a git worktree as a sibling dir. Each worktree is an isolated branch
# checkout for a concurrent chat, sharing one .git. Build dirs stay per-worktree.
#
# Usage: tools/qp-worktree.sh <branch> [base]
#   <branch>  new branch, worktree lands at ../quant-platform-<branch>
#   [base]    branch to fork from, default main
set -euo pipefail

branch="$1"
base="${2:-main}"
repo="$(git rev-parse --show-toplevel)"
wt="$(dirname "$repo")/quant-platform-${branch//\//-}"

git -C "$repo" worktree add -b "$branch" "$wt" "$base"

echo "worktree ready: $wt"
echo "  branch $branch off $base"
echo "  first build: cmake --preset release && cmake --build build/release"
