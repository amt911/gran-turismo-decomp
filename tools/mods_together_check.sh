#!/usr/bin/env bash
# In-game check that every gt-mods mod works with all the others loaded: builds each mods repo, installs every mod directory
# (one with a mod.toml) with enabled = true into one mods dir, runs headless gt_play on the `chase` pad route with each mod's
# fake input / log env, and lets tools/mods_together_check.py judge run.log: all loaded, nothing skipped, the route finished,
# and every mod with a rule there logged that it acted. Several mods on one address chain (host/mod_hooks.hpp).
#   tools/mods_together_check.sh [--decomp off|on] [--seconds N]
# Env: GT_MODS_REPOS (space-separated mods repos, default ../gt-mods; an unmerged mod branch can be added as a second repo:
# a mod dir already taken from an earlier repo is ignored),
# BUILD (default build; pointing it at a gt_play from before hook chaining must FAIL), OUT (default
# recomp/analysis/mods_together_check, gitignored), MOD_CHECK_NO_LOCK=1 (the caller already holds tools/cpu_lock.sh).
# Exit: 0 pass, 1 check failed, 2 setup problem. The run is real-time: memory cgroup + the shared CPU lock.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
repos=${GT_MODS_REPOS:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mods_together_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac  # absolute OUT: the runs join "$repo/$out"
orig_args=("$@")
decomp=off seconds=300
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;;
    *) echo "mods_together_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mods_together_check: no $build/gt_play" >&2; exit 2; }
[ -f config/pad_routes/chase.pad ] || { echo "mods_together_check: no pad route chase" >&2; exit 2; }
# Build and install every mod (outside the lock: builds never take it), then re-run under the lock unless the caller holds it.
if [ -z "${MODS_TOGETHER_PREPARED:-}" ]; then
  rm -rf "$out/mods" && mkdir -p "$out/mods" || exit 2
  ids=() n=0
  for r in $repos; do
    n=$((n + 1)) mbuild="$out/mod-build-$n"
    { cmake -S "$r" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" && cmake --build "$mbuild" -j2; } \
      > "$out/mod-build-$n.log" 2>&1 || { echo "mods_together_check: build of $r failed (log $out/mod-build-$n.log)" >&2; exit 2; }
    for toml in "$r"/*/mod.toml; do
      [ -f "$toml" ] || continue
      d=$(basename "$(dirname "$toml")")
      lib=$(sed -n 's/^library *= *"\(.*\)".*/\1/p' "$toml")
      id=$(sed -n 's/^id *= *"\(.*\)".*/\1/p' "$toml")
      [ -f "$mbuild/$d/$lib" ] || { echo "mods_together_check: $d: no built $lib" >&2; exit 2; }
      if [ -e "$out/mods/$d" ]; then echo "mods_together_check: $d already taken from an earlier repo; $r's copy ignored"; continue; fi
      mkdir -p "$out/mods/$d" && cp "$mbuild/$d/$lib" "$out/mods/$d/" || exit 2
      sed 's/^enabled = false/enabled = true/' "$toml" > "$out/mods/$d/mod.toml"
      ids+=("$id")
    done
  done
  [ ${#ids[@]} -ge 2 ] || { echo "mods_together_check: fewer than two mods found in $repos" >&2; exit 2; }
  printf '%s\n' "${ids[@]}" > "$out/ids"
  if [ -z "${MOD_CHECK_NO_LOCK:-}" ]; then
    exec env MOD_CHECK_NO_LOCK=1 MODS_TOGETHER_PREPARED=1 tools/cpu_lock.sh mods_together_check "$0" "${orig_args[@]}"
  fi
fi
mapfile -t ids < "$out/ids"
pad=$(grep -v '^#' config/pad_routes/chase.pad)
run="$out/run_$decomp"
rm -rf "$run" && mkdir -p "$run/savedata" || exit 2
env GT_MODS_DIR="$repo/$out/mods" GT_SAVEDATA_DIR="$repo/$run/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 \
    GT_MOD_PEDALS_FAKE=0.5,0.3 GT_MOD_PEDALS_LOG=1 GT_MOD_CAMERA_FAKE=90 GT_MOD_CAMERA_LOG=1 GT_MOD_DRAWDIST_LOG=1 GT_MOD_RESET_LOG=1 GT_MOD_ROLLOVER_LOG=1 GT_MOD_ROLLOVER_FAKE=900:5 GT_MOD_CHEATS_SLOWMO=0.5 GT_MOD_CHEATS_FREEZE_AI=1 GT_MOD_CHEATS_LOG=1 \
    GT_PAD="$pad" GT_PAD_LOG=1 \
  systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
  timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$run/run.log" 2>&1
rc=$?
grep -h '^\[mods\]' "$run/run.log"
[ $rc -eq 0 ] || echo "mods_together_check: gt_play exited $rc (log $run/run.log)"
.venv/bin/python -I tools/mods_together_check.py "$run/run.log" "${ids[@]}" || exit 1
[ $rc -eq 0 ] || exit 1
