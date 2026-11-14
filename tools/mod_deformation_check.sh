#!/usr/bin/env bash
# In-game check of the deformation mod (gt-mods repo), v0.1.0: what exists today is the HIT detection from the velocity change (no mesh is
# changed yet: docs/FINDINGS.md "The real GE blob layout ... 2026-11-14"). Headless gt_play on the `rollover` route (the race; the car hits the
# barrier at step ~1830), the mod's log lines and a judge in this file.
#   tools/mod_deformation_check.sh [--decomp off|on] [--seconds N (default 100)] [--without-mod|--sabotage-threshold0|--others]
#   (default)  PASS only if no HIT before step 1500 (normal driving) and at least one HIT at the barrier (step >= 1700)
#   --others   the same with analog-pedals, cheats, draw-distance, orbit-camera, reset-to-track and rollover enabled too (every other mod)
#   Each of these must FAIL: --without-mod (the mod absent: no HIT at the barrier), --sabotage-threshold0 (threshold = 0: hits in normal driving)
# NOT covered, because the dent itself does not exist yet: frames identical before the hit, the player's screen box changing after it, an AI car of
# the same model unchanged, byte-identical with the mod off, and the zero-strength sabotage "changed after hit" (spec "Testing").
# Env: GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_deformation_check). Exit: 0 pass, 1 check failed, 2 setup/run problem.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_deformation_check}
decomp=off seconds=100 without=0 threshold=4.0 others=0
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;;
    --without-mod) without=1;; --sabotage-threshold0) threshold=0.0;; --others) others=1;;
    *) echo "mod_deformation_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_deformation_check: no $build/gt_play" >&2; exit 2; }
route=config/pad_routes/rollover.pad
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
fi
if [ $others -eq 1 ]; then
  for pair in analog-pedals:libanalog_pedals.so cheats:libcheats.so draw-distance:libdraw_distance.so orbit-camera:liborbit_camera.so \
              reset-to-track:libreset_to_track.so rollover:librollover.so; do
    install_mod "${pair%%:*}" "${pair#*:}" || { echo "mod_deformation_check: cannot install ${pair%%:*}" >&2; exit 2; }
  done
fi
log="$out/run.log"
env GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/save" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_PAD="$(grep -v '^#' "$route")" \
  tools/cpu_lock.sh "mod_deformation_check" systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
  timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$log" 2>&1
rc=$?
[ $rc -eq 0 ] || { echo "mod_deformation_check: gt_play exited $rc (log $log)" >&2; exit 2; }
grep -aq 'script done' "$log" || { echo "mod_deformation_check: the pad route did not finish (log $log)" >&2; exit 2; }
hits=$(grep -ao 'HIT step=[0-9]*' "$log" | sed 's/HIT step=//')
early=$(echo "$hits" | awk 'NF && $1 < 1500' | wc -l)
late=$(echo "$hits" | awk 'NF && $1 >= 1700' | wc -l)
echo "mod_deformation_check: decomp=$decomp hits=$(echo "$hits" | awk 'NF' | wc -l) before_step_1500=$early at_barrier=$late"
if [ "$early" -eq 0 ] && [ "$late" -ge 1 ]; then echo "mod_deformation_check: PASS"; exit 0; fi
echo "mod_deformation_check: FAIL (want 0 hits before step 1500 and at least 1 at the barrier)"; exit 1
