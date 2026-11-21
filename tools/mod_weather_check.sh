#!/usr/bin/env bash
# In-game check of the weather mod (gt-mods repo): headless gt_play on the `chase` pad route with frames dumped (GT_FRAME_DUMP), the mod's log (GT_MOD_WEATHER_LOG=1) and
# tools/mod_weather_check.py as judge. The scene grade is applied to dumped frames, so a graded frame is predicted from the baseline frame of the same present (see the judge's header).
#   tools/mod_weather_check.sh [--decomp off|on] [--seconds N] [--others] [--idle] [--dusk] [--progressive] [--rain] [--without-mod|--sabotage-nograde|--sabotage-norain]
#   (default)    off (mod installed, switched off) + idle (noon, no rain: every frame identical to off) + dusk + progressive + rain (heavy). PASS only if every judge passes.
#   --others     every other mod enabled too (the run must still pass)
#   Each of these must FAIL: --without-mod (no mod: the frames are not graded), --sabotage-nograde (-DWEATHER_SABOTAGE=1: the grade is computed and logged but never applied; with --dusk),
#   --sabotage-norain (=2: the rain draws nothing; with --rain)
# Env: GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_weather_check, gitignored; never /tmp), MOD_CHECK_NO_LOCK=1 (the caller holds
# tools/cpu_lock.sh), KEEP_BASELINE=1 (reuse `off` of $OUT when --decomp, --seconds and --others match). Exit: 0 pass, 1 check failed, 2 setup/run problem.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_weather_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac
decomp=off seconds=450 every=30 others=0 without=0 sab=0
want=""
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --others) others=1;;
    --idle|--dusk|--progressive|--rain) want="$want ${1#--}";;
    --without-mod) without=1;; --sabotage-nograde) sab=1;; --sabotage-norain) sab=2;;
    *) echo "mod_weather_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_weather_check: no $build/gt_play" >&2; exit 2; }
[ -z "$want" ] && want=" idle dusk progressive rain"
has() { case "$want" in *" $1"*) return 0;; *) return 1;; esac; }
route=config/pad_routes/chase.pad
[ -f "$route" ] || { echo "mod_weather_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out" || exit 2
mbuild="$out/mod-build-s$sab"
flags=""; [ $sab -ne 0 ] && flags="-DWEATHER_SABOTAGE=$sab"
{ cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" && cmake --build "$mbuild" -j2; } > "$out/mod-build-s$sab.log" 2>&1 \
  || { echo "mod_weather_check: mod build failed (log $out/mod-build-s$sab.log)" >&2; exit 2; }
install_mod() {  # install_mod MODS_DIR DIRNAME LIBFILE ENABLED
  mkdir -p "$1/$2" && cp "$mbuild/$2/$3" "$1/$2/" && sed "s/^enabled = false/enabled = $4/" "$mods_repo/$2/mod.toml" > "$1/$2/mod.toml"
}
make_mods() {
  rm -rf "$1" && mkdir -p "$1" || exit 2
  [ $without -eq 0 ] && { install_mod "$1" weather libweather.so "$2" || exit 2; }
  if [ $others -eq 1 ]; then
    for pair in analog-pedals:libanalog_pedals.so cheats:libcheats.so draw-distance:libdraw_distance.so orbit-camera:liborbit_camera.so reset-to-track:libreset_to_track.so \
                rollover:librollover.so deformation:libdeformation.so mechanical-damage:libmechanical_damage.so extra-cameras:libextra_cameras.so; do
      [ -d "$mods_repo/${pair%%:*}" ] || continue
      install_mod "$1" "${pair%%:*}" "${pair#*:}" true || { echo "mod_weather_check: cannot install ${pair%%:*}" >&2; exit 2; }
    done
  fi
}
declare -a extra_env=()
run_one() {  # run_one LABEL MODS_DIR
  local label=$1 mdir=$2
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_MOD_WEATHER_LOG=1
                 GT_PAD="$(grep -v '^#' "$route")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every GT_FRAME_DUMP="$repo/$out/$label/frames")
  vars+=("${extra_env[@]}")
  local -a lock=(tools/cpu_lock.sh "mod_weather_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do
    "${lock[@]}" env "${vars[@]}" systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?
    [ $rc -ne 143 ] && break
    echo "mod_weather_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  [ $rc -eq 0 ] || { echo "mod_weather_check: $label: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; }
  grep -aq 'script done' "$out/$label/run.log" || { echo "mod_weather_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; }
  grep -ah '^\[mods\]' "$out/$label/run.log" | head -2
}
py=.venv/bin/python
make_mods "$out/mods_off" false
make_mods "$out/mods" true
rc=0
judge() { $py -I tools/mod_weather_check.py "$out" "$1" || rc=1; }
cfg="decomp=$decomp seconds=$seconds others=$others"
if [ -n "${KEEP_BASELINE:-}" ] && [ "$(cat "$out/.config" 2>/dev/null)" = "$cfg" ] && [ -d "$out/off/frames" ]; then :; else
  extra_env=(); run_one off "$out/mods_off" || exit 2
fi
echo "$cfg" > "$out/.config"
if has idle; then extra_env=(GT_MOD_WEATHER_PRESET=noon GT_MOD_WEATHER_RAIN=0); run_one idle "$out/mods" || exit 2; judge idle; fi
if has dusk; then extra_env=(GT_MOD_WEATHER_PRESET=dusk GT_MOD_WEATHER_RAIN=0); run_one dusk "$out/mods" || exit 2; judge dusk; fi
if has progressive; then extra_env=(GT_MOD_WEATHER_PRESET=progressive GT_MOD_WEATHER_RAIN=0 GT_MOD_WEATHER_STEPS=1800); run_one progressive "$out/mods" || exit 2; judge progressive; fi
if has rain; then extra_env=(GT_MOD_WEATHER_PRESET=noon GT_MOD_WEATHER_RAIN=3); run_one rain "$out/mods" || exit 2; judge rain; fi
[ $rc -eq 0 ] && echo "mod_weather_check: PASS" || echo "mod_weather_check: FAIL"
exit $rc
