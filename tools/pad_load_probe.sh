#!/usr/bin/env bash
# Runs pad routes side by side like a full gate (diff mode, real time, GT_PAD_EXIT_AFTER tail) with GT_IO_LOG=1, to
# measure how a load stretches the pages' timing: every "[io] seek ... vblank N" and "[pad] WAIT ... waited N" line.
#   tools/pad_load_probe.sh <tag> <route[,route...]> [busy_workers]
# Env: GT_PAD_SETTLE (quiet vblanks after a seek WAIT, default the binary's); IO_LOAD=1 adds a loop reading big files.
# Output: recomp/analysis/padprobe_<tag>_<route>.{log,exit}. Real-time: run it under tools/cpu_lock.sh and taskset,
# e.g.  taskset -c 6-11,18-23 tools/cpu_lock.sh "pad probe" tools/pad_load_probe.sh t1 manual,garage 12
set -uo pipefail
tag=$1 routes=${2//,/ } busy=${3:-0}
out=recomp/analysis
.venv/bin/python -c "
from pathlib import Path
from tools.decomp_gate import prepare_saves
prepare_saves('$tag', Path('$out'), 'full', run='probe_$tag')" || exit 2
pids=()
for _ in $(seq 1 "$busy"); do (while :; do :; done) & pids+=($!); done
if [ -n "${IO_LOAD:-}" ]; then
  (while :; do find extracted -type f -size +1M -print0 2>/dev/null | xargs -0 -r cat > /dev/null 2>&1; done) & pids+=($!)
fi
cleanup() { [ ${#pids[@]} -gt 0 ] && kill "${pids[@]}" 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT
cmd=""
for route in $routes; do
  save=""
  case $route in save_*) save="GT_SAVEDATA_DIR=$(pwd)/$out/savedata_probe_${tag}_$route ";; esac
  cmd+="( ${save}GT_IO_LOG=1 GT_PAD_TRACE=1 GT_PAD_EXIT_AFTER=100 GT_PAD_LOG=1 GT_PAD=\"\$(grep -v '^#' config/pad_routes/$route.pad)\" \
timeout 1500 build/gt_native --max-seconds 1400 --decomp diff > $out/padprobe_${tag}_$route.log 2>&1; \
echo \$? > $out/padprobe_${tag}_$route.exit ) & "
done
systemd-run --user --scope --quiet --slice=gt-heavy.slice -- bash -c "$cmd wait"
for route in $routes; do echo "$route exit $(cat $out/padprobe_${tag}_$route.exit)"; done
