#!/usr/bin/env bash
# In-game check of the extra-cameras mod (gt-mods repo): headless gt_play on the `chase` pad route (the race, then SELECT presses that end in the chase view), the mod's log
# (GT_MOD_CAMS_LOG=1: the pose it set and, from the game's own world matrices, where the car was drawn) and tools/mod_cameras_check.py as judge. A view is chosen with
# GT_MOD_CAMS_VIEW=wheel|roof|tv|heli (the checks have no keyboard).
#   tools/mod_cameras_check.sh [--decomp off|on] [--seconds N] [--others] [--idle] [--wheel] [--roof] [--tv] [--heli] [--without-mod|--sabotage-offset]
#   (default)    off (mod installed, switched off) + idle (enabled, no extra view: every frame identical to off) + the wheel, roof and TV views. PASS only if every judge passes.
#   --cycle      the route's three SELECT presses with no forced view: the extra views come after the game's four (the third press reaches the Wheel view), the game's list stays on its first view
#   --heli       the `circuit` route (race, leave from the pause menu, the replay plays) with the helicopter camera: the eye stays above and behind the car in the replay
#   --others     every other mod enabled too (the run must still pass)
#   Each of these must FAIL: --without-mod (no mod installed: no CAM lines), --sabotage-cycle (-DCAMS_SABOTAGE=2: the presses are not followed; with --cycle), --sabotage-offset (-DCAMS_SABOTAGE=1: the pose set is the game's own: the car is not where the view says)
# Env: GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_cameras_check, gitignored; never /tmp), MOD_CHECK_NO_LOCK=1 (the caller holds tools/cpu_lock.sh),
# KEEP_BASELINE=1 (reuse off/idle of $OUT when --decomp and --seconds match). Exit: 0 pass, 1 check failed, 2 setup/run problem.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_cameras_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac
decomp=off seconds=170 every=30 others=0 without=0 sab=0
want=""
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --others) others=1;;
    --idle|--wheel|--roof|--tv|--heli|--cycle) want="$want ${1#--}";;
    --without-mod) without=1;; --sabotage-offset) sab=1;; --sabotage-cycle) sab=2;;
    *) echo "mod_cameras_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_cameras_check: no $build/gt_play" >&2; exit 2; }
[ -z "$want" ] && want=" idle wheel roof tv"
has() { case "$want" in *" $1"*) return 0;; *) return 1;; esac; }
route=config/pad_routes/chase.pad
has heli && { route=config/pad_routes/circuit.pad; [ "$seconds" = 170 ] && seconds=330; }
[ -f "$route" ] || { echo "mod_cameras_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out" || exit 2
mbuild="$out/mod-build-s$sab"
flags=""; [ $sab -ne 0 ] && flags="-DCAMS_SABOTAGE=$sab"
{ cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" && cmake --build "$mbuild" -j2; } > "$out/mod-build-s$sab.log" 2>&1 \
  || { echo "mod_cameras_check: mod build failed (log $out/mod-build-s$sab.log)" >&2; exit 2; }
install_mod() {  # install_mod MODS_DIR DIRNAME LIBFILE ENABLED
  mkdir -p "$1/$2" && cp "$mbuild/$2/$3" "$1/$2/" && sed "s/^enabled = false/enabled = $4/" "$mods_repo/$2/mod.toml" > "$1/$2/mod.toml"
}
make_mods() {
  rm -rf "$1" && mkdir -p "$1" || exit 2
  [ $without -eq 0 ] && { install_mod "$1" extra-cameras libextra_cameras.so "$2" || exit 2; }
  if [ $others -eq 1 ]; then
    for pair in analog-pedals:libanalog_pedals.so cheats:libcheats.so draw-distance:libdraw_distance.so orbit-camera:liborbit_camera.so reset-to-track:libreset_to_track.so \
                rollover:librollover.so deformation:libdeformation.so mechanical-damage:libmechanical_damage.so; do
      [ -d "$mods_repo/${pair%%:*}" ] || continue   # a mod that is not in this gt-mods checkout yet (mechanical-damage before its PR merges)
      install_mod "$1" "${pair%%:*}" "${pair#*:}" true || { echo "mod_cameras_check: cannot install ${pair%%:*}" >&2; exit 2; }
    done
  fi
}
declare -a extra_env=()
run_one() {  # run_one LABEL MODS_DIR FRAMES(0|1)
  local label=$1 mdir=$2 frames=$3
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_MOD_CAMS_LOG=1
                 GT_PAD="$(grep -v '^#' "$route")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every)
  vars+=("${extra_env[@]}")
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  local -a lock=(tools/cpu_lock.sh "mod_cameras_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do
    "${lock[@]}" env "${vars[@]}" systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?
    [ $rc -ne 143 ] && break
    echo "mod_cameras_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  [ $rc -eq 0 ] || { echo "mod_cameras_check: $label: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; }
  grep -aq 'script done' "$out/$label/run.log" || { echo "mod_cameras_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; }
  grep -ah '^\[mods\]' "$out/$label/run.log" | head -3
}
py=.venv/bin/python
make_mods "$out/mods_off" false
make_mods "$out/mods" true
rc=0
judge() { $py -I tools/mod_cameras_check.py "$out" "$1" || rc=1; }
cfg="decomp=$decomp seconds=$seconds others=$others route=$route"
reuse_ok() { [ -n "${KEEP_BASELINE:-}" ] && [ "$(cat "$out/.config" 2>/dev/null)" = "$cfg" ] && [ -d "$out/$1/frames" ]; }
if has cycle; then  # the game's own SELECT presses of the route, no forced view
  extra_env=()
  run_one cycle "$out/mods" 0 || exit 2
  judge cycle
  exit $rc
fi
if has heli; then
  extra_env=(GT_MOD_CAMS_VIEW=heli)
  run_one heli "$out/mods" 0 || exit 2
  judge heli
  exit $rc
fi
extra_env=()
reuse_ok off || run_one off "$out/mods_off" 1 || exit 2
run_one idle "$out/mods" 1 || exit 2
echo "$cfg" > "$out/.config"
has idle && judge idle
for v in wheel roof tv; do
  has $v || continue
  extra_env=(GT_MOD_CAMS_VIEW=$v)
  run_one $v "$out/mods" 1 || exit 2
  judge $v
done
[ $rc -eq 0 ] && echo "mod_cameras_check: PASS" || echo "mod_cameras_check: FAIL"
exit $rc
