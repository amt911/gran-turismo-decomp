#!/usr/bin/env bash
# In-game check of the ghost-any-mode mod (gt-mods repo): headless gt_play on the `chase` pad route with frames dumped every 6 vblanks (GT_FRAME_DUMP), the mod's log (GT_MOD_GHOST_LOG=1) and
# tools/mod_ghost_check.py as judge. The route cannot drive a lap: GT_MOD_GHOST_FAKE_LAP=m90 makes a lap boundary every 90 physics steps after the car first moves (it creeps at 1-3 m/s before the
# start), so the ghost of lap 2 trails the player by ~2 m, between the chase camera and the car.
#   tools/mod_ghost_check.sh [--decomp off|on] [--seconds N] [--others] [--idle] [--ghost] [--without-mod|--sabotage-current]
#   (default)    off (mod installed, switched off) + idle (the ghost hidden: every frame identical to off) + ghost (laps counted, the ghost replays the recording and trails the player, the projected box is
#                where the frames change, the pixels are a translucent blend). PASS only if every judge passes.
#   --others     every other mod enabled too (the run must still pass)
#   Each of these must FAIL: --without-mod (no mod: no LAP lines), --sabotage-current (-DGHOST_SABOTAGE=1: the ghost is drawn at the CURRENT pose: it sits on the player; with --ghost)
# Env: GT_MODS_REPO (default ../gt-mods), GT_SDK_DIR (default include/ of this checkout: the mod needs the v9 header), BUILD (default build), OUT (default recomp/analysis/mod_ghost_check, gitignored;
# never /tmp), MOD_CHECK_NO_LOCK=1 (the caller holds tools/cpu_lock.sh), KEEP_BASELINE=1 (reuse `off` of $OUT when --decomp, --seconds and --others match). Exit: 0 pass, 1 check failed, 2 setup/run problem.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_ghost_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac
decomp=off seconds=450 every=6 others=0 without=0 sab=0
sdk=${GT_SDK_DIR:-$repo/include}
want=""
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --others) others=1;;
    --idle|--ghost) want="$want ${1#--}";;
    --without-mod) without=1;; --sabotage-current) sab=1;;
    *) echo "mod_ghost_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_ghost_check: no $build/gt_play" >&2; exit 2; }
[ -z "$want" ] && want=" idle ghost"
has() { case "$want" in *" $1"*) return 0;; *) return 1;; esac; }
route=config/pad_routes/chase.pad
[ -f "$route" ] || { echo "mod_ghost_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out" || exit 2
mbuild="$out/mod-build-s$sab"
flags=""; [ $sab -ne 0 ] && flags="-DGHOST_SABOTAGE=$sab"
{ cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$sdk" "-DCMAKE_CXX_FLAGS=$flags" && cmake --build "$mbuild" -j2; } > "$out/mod-build-s$sab.log" 2>&1 \
  || { echo "mod_ghost_check: mod build failed (log $out/mod-build-s$sab.log)" >&2; exit 2; }
install_mod() {  # install_mod MODS_DIR DIRNAME LIBFILE ENABLED
  mkdir -p "$1/$2" && cp "$mbuild/$2/$3" "$1/$2/" && sed "s/^enabled = false/enabled = $4/" "$mods_repo/$2/mod.toml" > "$1/$2/mod.toml"
}
make_mods() {
  rm -rf "$1" && mkdir -p "$1" || exit 2
  [ $without -eq 0 ] && { install_mod "$1" ghost-any-mode libghost_any_mode.so "$2" || exit 2; }
  if [ $others -eq 1 ]; then
    for pair in analog-pedals:libanalog_pedals.so cheats:libcheats.so draw-distance:libdraw_distance.so orbit-camera:liborbit_camera.so reset-to-track:libreset_to_track.so \
                rollover:librollover.so deformation:libdeformation.so mechanical-damage:libmechanical_damage.so extra-cameras:libextra_cameras.so weather:libweather.so; do
      [ -d "$mods_repo/${pair%%:*}" ] || continue
      install_mod "$1" "${pair%%:*}" "${pair#*:}" true || { echo "mod_ghost_check: cannot install ${pair%%:*}" >&2; exit 2; }
    done
  fi
}
declare -a extra_env=()
run_one() {  # run_one LABEL MODS_DIR
  local label=$1 mdir=$2
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_MOD_GHOST_LOG=1
                 GT_PAD="$(grep -v '^#' "$route")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every GT_FRAME_DUMP="$repo/$out/$label/frames")
  vars+=("${extra_env[@]}")
  local -a lock=(tools/cpu_lock.sh "mod_ghost_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do
    "${lock[@]}" env "${vars[@]}" systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?
    [ $rc -ne 143 ] && break
    echo "mod_ghost_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  [ $rc -eq 0 ] || { echo "mod_ghost_check: $label: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; }
  grep -aq 'script done' "$out/$label/run.log" || { echo "mod_ghost_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; }
  grep -ah '^\[mods\]' "$out/$label/run.log" | head -2
}
py=.venv/bin/python
make_mods "$out/mods_off" false
make_mods "$out/mods" true
rc=0
judge() { $py -I tools/mod_ghost_check.py "$out" "$1" || rc=1; }
cfg="decomp=$decomp seconds=$seconds others=$others"
if [ -n "${KEEP_BASELINE:-}" ] && [ "$(cat "$out/.config" 2>/dev/null)" = "$cfg" ] && [ -d "$out/off/frames" ]; then :; else
  extra_env=(); run_one off "$out/mods_off" || exit 2
fi
echo "$cfg" > "$out/.config"
if has idle; then extra_env=(GT_MOD_GHOST_HIDE=1 GT_MOD_GHOST_FAKE_LAP=m90); run_one idle "$out/mods" || exit 2; judge idle; fi
if has ghost; then extra_env=(GT_MOD_GHOST_FAKE_LAP=m90); run_one ghost "$out/mods" || exit 2; judge ghost; fi
[ $rc -eq 0 ] && echo "mod_ghost_check: PASS" || echo "mod_ghost_check: FAIL"
exit $rc
