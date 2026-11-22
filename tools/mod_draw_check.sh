#!/usr/bin/env bash
# In-game check of the mod draw API (API 8: on_overlay, draw_text / draw_rect / draw_line, scene_grade): headless gt_play on the `chase` pad route
# with the probe mod tools/mod_draw_probe, frames dumped to disk (they carry the layer and the grade: host/play_backends.cpp frame_composer).
# Runs: baseline (probe installed, switched off), idle (on, drawing nothing: byte-identical), neutral (scene_grade(1,1,1,1,1) called: byte-identical), draw (shapes), grade (scene_grade 0.5). tools/mod_draw_check.py judges.
#   tools/mod_draw_check.sh [--sabotage-flush|--sabotage-grade] [--decomp off|on] [--seconds N] [--every N] [--only "idle neutral draw grade"] [--keep-baseline]
#   --sabotage-flush  rebuilds gt_play with -DGT_DRAW_SABOTAGE_NO_FLUSH=ON (the lists are dropped): the draw run must FAIL
#   --sabotage-grade  rebuilds gt_play with -DGT_GRADE_SABOTAGE_IGNORE=ON (the grade is dropped): the grade run must FAIL
# Env: MOD_CHECK_NO_LOCK=1, BUILD (default build), OUT (default recomp/analysis/mod_draw_check, gitignored). Exit: 0 pass, 1 check failed, 2 setup/run problem.
# Each run is a real-time job inside the memory cgroup and its own hold of tools/cpu_lock.sh.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_draw_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac
orig_args=("$@")
decomp=off seconds=420 every=30 sab="" only="idle neutral draw grade" keep=0
while [ $# -gt 0 ]; do
  case $1 in
    --sabotage-flush) sab=flush; only="draw";; --sabotage-grade) sab=grade; only="grade";; --keep-baseline) keep=1;;
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --every) every=$2; shift;; --only) only=$2; shift;;
    *) echo "mod_draw_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_draw_check: no $build/gt_play" >&2; exit 2; }
[ -f config/pad_routes/chase.pad ] || { echo "mod_draw_check: no pad route chase" >&2; exit 2; }
mkdir -p "$out" || exit 2
pad=$(grep -v '^#' config/pad_routes/chase.pad)
# mods_<label>/draw-probe: the library and a mod.toml with the label's settings
make_mods() {  # LABEL ENABLED DRAW GRADE
  local d="$out/mods_$1/draw-probe"
  rm -rf "$d" && mkdir -p "$d" || return 2
  cp "$out/libdraw_probe.so" "$d/" || return 2
  sed -e "s/^enabled = true/enabled = $2/" -e "s/^draw = 1/draw = $3/" -e "s/^grade = \"\"/grade = \"$4\"/" tools/mod_draw_probe/mod.toml > "$d/mod.toml"
}
if [ -z "${MOD_CHECK_ONLY:-}" ]; then
  clang++ -std=c++20 -shared -fPIC -O1 -Wall -Wextra -I include tools/mod_draw_probe/probe.cpp -o "$out/libdraw_probe.so" || { echo "mod_draw_check: probe build failed" >&2; exit 2; }
  make_mods baseline false 1 "" && make_mods idle true 0 "" && make_mods neutral true 0 "1 1 1 1 1" && make_mods draw true 1 "" && make_mods grade true 0 "0.5 0.5 0.5 1 1" || exit 2
fi
run_one() {  # LABEL
  local label=$1 mdir="$out/mods_$1"
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  env GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_PAD="$pad" GT_PAD_LOG=1 \
      GT_FRAME_DUMP="$repo/$out/$label/frames" GT_FRAME_EVERY=$every \
    systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
    timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
  local rc=$?
  if [ $rc -ne 0 ]; then echo "mod_draw_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  grep -h '^\[mods\]\|^\[drawprobe\]' "$out/$label/run.log"; grep -h 'script done' "$out/$label/run.log"
}
if [ -n "${MOD_CHECK_ONLY:-}" ]; then run_one "$MOD_CHECK_ONLY"; exit $?; fi
locked_run_one() {
  if [ -n "${MOD_CHECK_NO_LOCK:-}" ]; then MOD_CHECK_ONLY=$1 run_one "$1"; return $?; fi
  tools/cpu_lock.sh "mod_draw_check:$1" env MOD_CHECK_ONLY="$1" OUT="$out" BUILD="$build" "$0" "${orig_args[@]}"
}
restore() { cmake -S . -B "$build" -DGT_DRAW_SABOTAGE_NO_FLUSH=OFF -DGT_GRADE_SABOTAGE_IGNORE=OFF > /dev/null 2>&1 && cmake --build "$build" -j4 --target gt_play > /dev/null 2>&1; }
if [ -n "$sab" ]; then
  flag=GT_DRAW_SABOTAGE_NO_FLUSH; [ "$sab" = grade ] && flag=GT_GRADE_SABOTAGE_IGNORE
  trap restore EXIT
  { cmake -S . -B "$build" -D$flag=ON && cmake --build "$build" -j4 --target gt_play; } > "$out/gt_play-sabotage-build.log" 2>&1 \
    || { echo "mod_draw_check: sabotage build failed (log $out/gt_play-sabotage-build.log)" >&2; exit 2; }
fi
if [ $keep -eq 0 ] || [ ! -d "$out/baseline/frames" ]; then locked_run_one baseline || exit 2; fi
for label in $only; do locked_run_one "$label" || exit 2; done
.venv/bin/python -I tools/mod_draw_check.py "$out" $only
