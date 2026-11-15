#!/usr/bin/env bash
# In-game check of the deformation mod (gt-mods repo). Headless gt_play on the `deformation` route (rollover + a SELECT to the chase view) (the race; the car hits the barrier at step ~1830),
# frames every 30 vblanks, the mod's log and tools/mod_deformation_check.py as judge.
#   tools/mod_deformation_check.sh [--decomp off|on] [--seconds N (default 150)] [--others] [--without-mod|--sabotage-zero|--sabotage-threshold0]
#   (default)  baseline (the mod installed, switched off) + the mod enabled + the switched-off run again. PASS only if: no HIT before step 1500 and one at the barrier; the frames
#              before the hit are byte-identical to the baseline; after it they differ ("changed after hit"); a second switched-off run is
#              byte-identical to the baseline (mod off = deterministic, does nothing)
#   --others   the same with analog-pedals, cheats, draw-distance, orbit-camera, reset-to-track and rollover enabled too
#   Each of these must FAIL: --without-mod (no HIT, nothing changes), --sabotage-zero (depth_per_ms = 0: the dent has zero strength: "changed after hit"
#   fails), --sabotage-threshold0 (threshold = 0: hits in normal driving)
# Not judged (no way to tell from the frames): that an AI car of the same model stays intact; the mod's COPY line shows the player's parts only and it
# refuses a model drawn by more than one car.
# Env: GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_deformation_check). Exit: 0 pass, 1 check failed, 2 setup/run problem.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_deformation_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac  # absolute OUT: the runs join "$repo/$out"
decomp=off seconds=150 without=0 threshold=4.0 others=0 every=30 sabotage=""
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;;
    --without-mod) without=1;; --sabotage-threshold0) threshold=0.0; sabotage=1;; --sabotage-zero) sabotage=zero;; --others) others=1;;
    *) echo "mod_deformation_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_deformation_check: no $build/gt_play" >&2; exit 2; }
route=config/pad_routes/deformation.pad
mbuild="$out/mod-build"; mdir="$out/mods"
rm -rf "$mdir" && mkdir -p "$mdir" "$out/save" || exit 2
{ cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" && cmake --build "$mbuild" -j2; } > "$out/mod-build.log" 2>&1 \
  || { echo "mod_deformation_check: mod build failed (log $out/mod-build.log)" >&2; exit 2; }
install_mod() {  # install_mod DIRNAME LIBFILE
  mkdir -p "$mdir/$1" && cp "$mbuild/$1/$2" "$mdir/$1/" && sed 's/^enabled = false/enabled = true/' "$mods_repo/$1/mod.toml" > "$mdir/$1/mod.toml"
}
if [ $without -eq 0 ]; then
  install_mod deformation libdeformation.so || exit 2
  sed -i "s/^threshold *=.*/threshold = $threshold/" "$mdir/deformation/mod.toml"
  # The barrier hit is on the car's front-right and the chase camera looks at its rear: a dent on the front is behind the car. The check aims the same hit at the
  # rear-right FLANK instead (the forward component pushes along +x, the car's right; the right one along -z, the rear), which the camera sees edge-on.
  sed -i 's/^forward_axis *=.*/forward_axis = "x"/; s/^forward_sign *=.*/forward_sign = 1/; s/^right_axis *=.*/right_axis = "z"/; s/^right_sign *=.*/right_sign = -1/' "$mdir/deformation/mod.toml"
  [ "$sabotage" = zero ] && sed -i 's/^depth_per_ms *=.*/depth_per_ms = 0.0/' "$mdir/deformation/mod.toml"
fi
if [ $others -eq 1 ]; then
  for pair in analog-pedals:libanalog_pedals.so cheats:libcheats.so draw-distance:libdraw_distance.so orbit-camera:liborbit_camera.so \
              reset-to-track:libreset_to_track.so rollover:librollover.so; do
    install_mod "${pair%%:*}" "${pair#*:}" || { echo "mod_deformation_check: cannot install ${pair%%:*}" >&2; exit 2; }
  done
fi
# run_one LABEL MODS_DIR FRAMES(0|1): one real-time run of the rollover route inside the memory cgroup and the shared CPU lock
run_one() {
  local label=$1 mods=$2 frames=$3
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/save" || return 2
  local -a vars=(GT_MODS_DIR="$repo/$mods" GT_SAVEDATA_DIR="$repo/$out/$label/save" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_PAD="$(grep -v '^#' "$route")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every)
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  env "${vars[@]}" tools/cpu_lock.sh "mod_deformation_check:$label" systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
    timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
  local rc=$?
  [ $rc -eq 0 ] || { echo "mod_deformation_check: $label: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; }
  grep -aq 'script done' "$out/$label/run.log" || { echo "mod_deformation_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; }
}
need_baseline() { [ -z "${KEEP_BASELINE:-}" ] || [ ! -d "$out/baseline/frames" ] || [ "$(cat "$out/baseline/.config" 2>/dev/null)" != "decomp=$decomp seconds=$seconds every=$every others=$others" ]; }
# The baseline is the run with the mod installed but switched off, not a run with an empty mods dir: the host's mod loader alone (a non-empty mods dir)
# shifts the race by a few steps against real time (measured 2026-11-14: two empty-dir runs agree, two switched-off runs agree, the two kinds differ
# from vblank 3330 on), so only runs with the same mods dir layout are comparable frame for frame.
mkdir -p "$out/mods_off/deformation" && cp "$mbuild/deformation/libdeformation.so" "$out/mods_off/deformation/" \
  && sed 's/^enabled = true/enabled = false/; s/^enabled = false/enabled = false/' "$mods_repo/deformation/mod.toml" > "$out/mods_off/deformation/mod.toml" || exit 2
if [ $others -eq 1 ]; then  # the other mods are enabled in the baseline too: only the deformation mod differs between the runs
  for d in analog-pedals cheats draw-distance orbit-camera reset-to-track rollover; do rm -rf "$out/mods_off/$d" && cp -r "$mdir/$d" "$out/mods_off/$d"; done
fi
if need_baseline; then run_one baseline "$out/mods_off" 1 || exit 2; echo "decomp=$decomp seconds=$seconds every=$every others=$others" > "$out/baseline/.config"; fi
run_one run "$mdir" 1 || exit 2
hits=$(grep -ao 'HIT step=[0-9]*' "$out/run/run.log" | sed 's/HIT step=//')
early=$(echo "$hits" | awk 'NF && $1 < 1500' | wc -l)
late=$(echo "$hits" | awk 'NF && $1 >= 1700' | wc -l)
echo "mod_deformation_check: decomp=$decomp hits=$(echo "$hits" | awk 'NF' | wc -l) before_step_1500=$early at_barrier=$late"
grep -a "\[mod:deformation\] \(CAPTURE\|DENT\|RESTORE\)" "$out/run/run.log" | head -8
rc=0
[ "$early" -eq 0 ] && [ "$late" -ge 1 ] || { echo "mod_deformation_check: FAIL hit detection (want 0 hits before step 1500 and at least 1 at the barrier)"; rc=1; }
if [ $without -eq 0 ]; then
  # the same run with a dent of zero strength: the mod's own work (the copies, the redirected draws) shifts the race against real time after the
  # hit, so frames after it are compared with this run, which differs from the dented one only by the dent
  rm -rf "$out/mods_zero" && mkdir -p "$out/mods_zero" && cp -r "$mdir/." "$out/mods_zero/" && sed -i 's/^depth_per_ms *=.*/depth_per_ms = 0.0/' "$out/mods_zero/deformation/mod.toml"
  run_one zero "$out/mods_zero" 1 || exit 2
fi
python3 -I tools/mod_deformation_check.py "$out" deform || rc=1
if [ $without -eq 0 ] && [ -z "$sabotage" ]; then
  # the mod installed but switched off: byte-identical frames
  run_one identity "$out/mods_off" 1 || exit 2
  python3 -I tools/mod_deformation_check.py "$out" identity || rc=1
fi
[ $rc -eq 0 ] && echo "mod_deformation_check: PASS" || echo "mod_deformation_check: FAIL"
exit $rc
