#!/usr/bin/env bash
# Runs one pad route (config/pad_routes/<name>.pad, or a path) on build/gt_native inside the shared heavy-job slice,
# for exploring a route: frame dumps, RAM dumps and the [pad] log go under <outdir> (keep it in recomp/analysis/,
# the images and RAM are game output).
#   tools/pad_route.sh <route> <seconds> <outdir> [gt_native args, default: --decomp off]
# Environment passes through (GT_FRAME_EVERY, GT_RAM_DUMP, LCS_UNCAPPED=1 to run faster than real time, ...).
# <outdir>/run.log has stderr, <outdir>/run.exit the exit code (97 = a GT_PAD WAIT timed out).
set -euo pipefail
route=$1 seconds=$2 outdir=$3
shift 3
[ $# -gt 0 ] || set -- --decomp off
file=$route
[ -f "$file" ] || file="config/pad_routes/$route.pad"
mkdir -p "$outdir"
GT_PAD="$(grep -v '^#' "$file")"
export GT_PAD GT_PAD_LOG=1
export GT_FRAME_DUMP="${GT_FRAME_DUMP-$outdir/frames}"
set +e
systemd-run --user --scope --quiet --slice=gt-heavy.slice -- \
  timeout "$((seconds + 60))" build/gt_native --max-seconds "$seconds" "$@" > "$outdir/run.log" 2>&1
echo "$?" > "$outdir/run.exit"
