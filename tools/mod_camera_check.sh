#!/usr/bin/env bash
# In-game check of the orbit-camera mod (gt-mods repo): headless gt_play on the `chase` pad route (the race + three SELECT presses
# that cycle the camera view), frames dumped to disk. One run without the mod (the baseline) and one per fake angle with the mod
# installed and GT_MOD_CAMERA_FAKE=<yaw degrees> (180 = rear view; no stick needed). tools/mod_camera_check.py judges the frames:
#   yaw 0   frames byte-identical to the baseline (the mod writes nothing at the identity);
#   yaw != 0  the 3D area differs from the baseline, the HUD strip is identical, and where the baseline shows the player's car
#           (orange pixels near the centre) the car is still there, centred within tolerance (the pivot is the car).
#   tools/mod_camera_check.sh [--without-mod] [--sabotage] [--yaws "0 90 180"] [--decomp off|on] [--seconds N] [--every N] [--keep-baseline]
#   --without-mod  empty mods dir for the "mod" runs too (must FAIL: nothing changes the picture)
#   --sabotage     builds the mod with ORBIT_CAMERA_SABOTAGE=1 (pivot at the eye, not at the car; must FAIL the centred-car check)
#   --keep-baseline  reuse $OUT/baseline from an earlier run (same --decomp)
# Env: MOD_CHECK_NO_LOCK=1 (the caller already holds tools/cpu_lock.sh), GT_MODS_REPO (default ../gt-mods), BUILD (default build),
# OUT (default recomp/analysis/mod_camera_check, gitignored; frames are on disk there, never /tmp). Exit: 0 pass, 1 check failed, 2 setup/run problem.
# Each run is a real-time job: inside the memory cgroup and the shared CPU lock (tools/cpu_lock.sh).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_camera_check}
decomp=off seconds=300 every=30 without=0 sabotage=0 yaws="0 90 180" keep=0
while [ $# -gt 0 ]; do
  case $1 in
    --without-mod) without=1;; --sabotage) sabotage=1;; --keep-baseline) keep=1;;
    --yaws) yaws=$2; shift;; --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --every) every=$2; shift;;
    *) echo "mod_camera_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_camera_check: no $build/gt_play" >&2; exit 2; }
[ -f config/pad_routes/chase.pad ] || { echo "mod_camera_check: no pad route chase" >&2; exit 2; }
mkdir -p "$out" || exit 2
mkdir -p "$out/mods/orbit-camera" "$out/nomods"
if [ $without -eq 0 ]; then
  mbuild="$out/mod-build"
  flags=""; [ $sabotage -ne 0 ] && flags="-DORBIT_CAMERA_SABOTAGE=1"
  { cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" \
      && cmake --build "$mbuild" -j2 --target orbit_camera; } > "$out/mod-build.log" 2>&1 \
    || { echo "mod_camera_check: mod build failed (log $out/mod-build.log)" >&2; exit 2; }
  cp "$mbuild/orbit-camera/liborbit_camera.so" "$out/mods/orbit-camera/" || exit 2
  sed 's/^enabled = false/enabled = true/' "$mods_repo/orbit-camera/mod.toml" > "$out/mods/orbit-camera/mod.toml"
fi
pad=$(grep -v '^#' config/pad_routes/chase.pad)
# One lock hold for all the runs (each is real-time; the whole check stays well under the lock's 30 min): re-run this script under it.
if [ -z "${MOD_CHECK_NO_LOCK:-}" ]; then
  exec env MOD_CHECK_NO_LOCK=1 tools/cpu_lock.sh mod_camera_check "$0" "$@"
fi
# run_one LABEL MODS_DIR [FAKE]: one gt_play run, frames in $out/LABEL/frames
run_one() {
  local label=$1 mdir=$2 fake=${3:-}
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  env GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 \
      GT_MOD_CAMERA_FAKE="$fake" GT_MOD_CAMERA_LOG=1 GT_PAD="$pad" GT_PAD_LOG=1 GT_FRAME_DUMP="$repo/$out/$label/frames" GT_FRAME_EVERY=$every \
    systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
    timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
  local rc=$?
  if [ $rc -ne 0 ]; then echo "mod_camera_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  grep -h '^\[mods\]' "$out/$label/run.log"; grep -h 'script done' "$out/$label/run.log"
}
if [ $keep -eq 0 ] || [ ! -d "$out/baseline/frames" ]; then run_one baseline "$out/nomods" || exit 2; fi
mdir="$out/mods"; [ $without -eq 1 ] && mdir="$out/nomods"
for y in $yaws; do run_one "yaw_$y" "$mdir" "$y" || exit 2; done
args=""
for y in $yaws; do args="$args yaw_$y"; done
.venv/bin/python -I tools/mod_camera_check.py "$out" $args
