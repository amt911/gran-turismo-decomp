#!/usr/bin/env bash
# Like tools/pad_route.sh, but under gdb: stops at the first C++ exception (`catch throw`) and prints a backtrace
# to <outdir>/gdb.log, for a route that dies with "terminate called ..." (exit 134).
#   tools/pad_route_gdb.sh <route> <seconds> <outdir> [gt_native args, default: --decomp on]
set -euo pipefail
route=$1 seconds=$2 outdir=$3
shift 3
[ $# -gt 0 ] || set -- --decomp on
file=$route
[ -f "$file" ] || file="config/pad_routes/$route.pad"
mkdir -p "$outdir"
GT_PAD="$(grep -v '^#' "$file")"
export GT_PAD GT_PAD_LOG=1 LCS_UNCAPPED="${LCS_UNCAPPED-1}"
export GT_FRAME_DUMP="${GT_FRAME_DUMP-}"
unset GT_FRAME_DUMP
set +e
systemd-run --user --scope --quiet --slice=gt-heavy.slice -- \
  timeout "$((seconds + 120))" gdb -batch -ex "catch throw" -ex run -ex "bt 40" --args \
  build/gt_native --max-seconds "$seconds" "$@" > "$outdir/gdb.log" 2>&1
echo "$?" > "$outdir/gdb.exit"
