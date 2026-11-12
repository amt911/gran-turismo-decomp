#!/usr/bin/env bash
# In-game check of the draw-distance mod (gt-mods repo): headless gt_play on the `chase` pad route (the race), frames dumped to disk.
# One run without the mod (the baseline) and one per scale with the mod installed and GT_MOD_DRAWDIST_SCALE=<s>.
# tools/mod_drawdist_check.py judges the frames:
#   scale 1.0  frames byte-identical to the baseline (the mod writes nothing at the identity);
#   scale > 1  the 3D area changes (far scenery shows) but the near field, the HUD digits and the car are as in the baseline.
#   tools/mod_drawdist_check.sh [--without-mod] [--sabotage|--sabotage-mild] [--toggle-off [--toggle-at V]] [--scales "1 2.5"] [--decomp off|on] [--seconds N] [--every N] [--keep-baseline]
#   --without-mod  empty mods dir for the "mod" runs too (must FAIL: nothing changes the picture)
#   --sabotage     builds the mod with DRAW_DISTANCE_SABOTAGE=1 (near plane scaled by far_scale x 6 too; must FAIL the composition check)
#   --sabotage-mild  DRAW_DISTANCE_SABOTAGE=2 (near x 1.3 only; must FAIL too: the thresholds are tight enough to see a mild near change)
#   --toggle-off   the F9 menu's toggle turns the mod off mid-race (GT_MOD_MENU_TEST_TOGGLE=draw-distance@v<--toggle-at>, default v18500):
#                  one run at scale 2.5 (--scales is ignored); PASS = edited before, byte-identical to the baseline after
#   --keep-baseline  reuse $OUT/baseline from an earlier run (same --decomp)
# Env: MOD_CHECK_NO_LOCK=1 (the caller already holds tools/cpu_lock.sh), GT_MODS_REPO (default ../gt-mods), BUILD (default build),
# OUT (default recomp/analysis/mod_drawdist_check, gitignored; frames are on disk there, never /tmp). Exit: 0 pass, 1 check failed, 2 setup/run problem.
# Each run is a real-time job: inside the memory cgroup and the shared CPU lock (tools/cpu_lock.sh), one hold for all the runs.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_drawdist_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac  # absolute OUT: the runs join "$repo/$out"
orig_args=("$@")  # the parse below shifts them away; the lock re-exec needs them
decomp=off seconds=300 every=30 without=0 sabotage=0 scales="1 2.5" keep=0 toggle=0 toggle_at=18500
while [ $# -gt 0 ]; do
  case $1 in
    --without-mod) without=1;; --toggle-off) toggle=1;; --toggle-at) toggle_at=$2; shift;; --sabotage) sabotage=1;; --sabotage-mild) sabotage=2;; --keep-baseline) keep=1;;
    --scales) scales=$2; shift;; --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --every) every=$2; shift;;
    *) echo "mod_drawdist_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_drawdist_check: no $build/gt_play" >&2; exit 2; }
[ -f config/pad_routes/chase.pad ] || { echo "mod_drawdist_check: no pad route chase" >&2; exit 2; }
mkdir -p "$out" "$out/mods/draw-distance" "$out/nomods" || exit 2
if [ $without -eq 0 ]; then
  mbuild="$out/mod-build"
  flags=""; [ $sabotage -ne 0 ] && flags="-DDRAW_DISTANCE_SABOTAGE=$sabotage"
  { cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" \
      && cmake --build "$mbuild" -j2 --target draw_distance; } > "$out/mod-build.log" 2>&1 \
    || { echo "mod_drawdist_check: mod build failed (log $out/mod-build.log)" >&2; exit 2; }
  cp "$mbuild/draw-distance/libdraw_distance.so" "$out/mods/draw-distance/" || exit 2
  sed 's/^enabled = false/enabled = true/' "$mods_repo/draw-distance/mod.toml" > "$out/mods/draw-distance/mod.toml"
fi
pad=$(grep -v '^#' config/pad_routes/chase.pad)
# One lock hold for all the runs (each is real-time; the whole check stays well under the lock's 30 min): re-run this script under it.
if [ -z "${MOD_CHECK_NO_LOCK:-}" ]; then
  exec env MOD_CHECK_NO_LOCK=1 tools/cpu_lock.sh mod_drawdist_check "$0" "${orig_args[@]}"
fi
toggle_env=""; [ $toggle -eq 1 ] && toggle_env="draw-distance@v$toggle_at"
# run_one LABEL MODS_DIR [SCALE]: one gt_play run, frames in $out/LABEL/frames
run_one() {
  local label=$1 mdir=$2 scale=${3:-}
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  env GT_MOD_MENU_TEST_TOGGLE="$toggle_env" GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 \
      GT_MOD_DRAWDIST_SCALE="$scale" GT_MOD_DRAWDIST_LOG=1 GT_PAD="$pad" GT_PAD_LOG=1 GT_FRAME_DUMP="$repo/$out/$label/frames" GT_FRAME_EVERY=$every \
    systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
    timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
  local rc=$?
  if [ $rc -ne 0 ]; then echo "mod_drawdist_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  grep -h '^\[mods\]' "$out/$label/run.log"; grep -h 'script done' "$out/$label/run.log"
}
if [ $keep -eq 0 ] || [ ! -d "$out/baseline/frames" ]; then run_one baseline "$out/nomods" || exit 2; fi
mdir="$out/mods"; [ $without -eq 1 ] && mdir="$out/nomods"
if [ $toggle -eq 1 ]; then
  run_one toggle_2.5 "$mdir" 2.5 || exit 2
  .venv/bin/python -I tools/mod_drawdist_check.py "$out" toggle_2.5 --toggle-at "$toggle_at"
  exit $?
fi
args=""
for s in $scales; do run_one "scale_$s" "$mdir" "$s" || exit 2; args="$args scale_$s"; done
.venv/bin/python -I tools/mod_drawdist_check.py "$out" $args
