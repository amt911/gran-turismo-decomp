#!/usr/bin/env bash
# In-game check of the mechanical-damage mod (gt-mods repo): headless gt_play on the `rollover` pad route (the race, accelerator held from the start; the car
# accelerates from rest), the mod's per-step log (GT_MOD_DAMAGE_LOG=1) and tools/mod_damage_check.py as judge. The hits are the mod's own fake hits
# (GT_MOD_DAMAGE_FAKE=m30:<dv>:<f|b|r|l>, 30 steps after the car moves: the real barrier hit at step ~1850 comes too late to measure its effect).
#   tools/mod_damage_check.sh [--decomp off|on] [--seconds N] [--others] [--power] [--repair] [--cap] [--pull] [--replay] [--toggle-off]
#                             [--without-mod|--sabotage-scale0|--sabotage-liveonly]
#   (default)   off (mod installed, switched off) + idle (enabled, no hit: frames identical to off before the first hit) + front (power drop), repair, rear (speed cap),
#               right and left (yaw pull). PASS only if every judge passes. Each mode flag runs only its own judge (with the idle run it compares to).
#   --replay    the `circuit` route (race, leave from the pause menu, the replay plays) with a fake hit: the damage is rebuilt in the replay (same hit, same speed)
#   --toggle-off the F9 toggle (GT_MOD_MENU_TEST_TOGGLE) switches the mod off after a hit: it logs nothing afterwards
#   --others    every other mod enabled too (the run must still pass)
#   Each of these must FAIL: --without-mod (no mod installed: no HIT, no effect), --sabotage-scale0 (effect_scale = 0: the hit is logged, nothing changes),
#   --sabotage-liveonly (-DDAMAGE_SABOTAGE=1: the mod ignores the replay; with --replay)
# Env: GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_damage_check, gitignored; never /tmp), MOD_CHECK_NO_LOCK=1 (the caller holds
# tools/cpu_lock.sh), KEEP_BASELINE=1 (reuse off/idle of $OUT when --decomp and --seconds match). Exit: 0 pass, 1 check failed, 2 setup/run problem.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_damage_check}
case $out in /*) out=$(realpath -m --relative-to="$repo" "$out") || exit 2;; esac
decomp=off seconds=150 every=30 others=0 without=0 scale=1 sab=0
want=""  # the modes asked for; empty = all of the route ones
while [ $# -gt 0 ]; do
  case $1 in
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --others) others=1;;
    --idle|--power|--repair|--cap|--pull|--replay|--toggle-off) want="$want ${1#--}";;
    --without-mod) without=1;; --sabotage-scale0) scale=0;; --sabotage-liveonly) sab=1;;
    *) echo "mod_damage_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_damage_check: no $build/gt_play" >&2; exit 2; }
[ -z "$want" ] && want=" idle power repair cap pull"
has() { case "$want" in *" $1"*) return 0;; *) return 1;; esac; }
route=config/pad_routes/rollover.pad
has replay && { route=config/pad_routes/circuit.pad; [ "$seconds" = 150 ] && seconds=330; }
[ -f "$route" ] || { echo "mod_damage_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out" || exit 2
mbuild="$out/mod-build-s$sab"
flags=""; [ $sab -ne 0 ] && flags="-DDAMAGE_SABOTAGE=$sab"
{ cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" && cmake --build "$mbuild" -j2; } > "$out/mod-build-s$sab.log" 2>&1 \
  || { echo "mod_damage_check: mod build failed (log $out/mod-build-s$sab.log)" >&2; exit 2; }
install_mod() {  # install_mod MODS_DIR DIRNAME LIBFILE ENABLED
  mkdir -p "$1/$2" && cp "$mbuild/$2/$3" "$1/$2/" && sed "s/^enabled = false/enabled = $4/" "$mods_repo/$2/mod.toml" > "$1/$2/mod.toml"
}
make_mods() {  # make_mods DIR ENABLED: the damage mod (unless --without-mod) and, with --others, the other mods (always enabled)
  rm -rf "$1" && mkdir -p "$1" || exit 2
  [ $without -eq 0 ] && { install_mod "$1" mechanical-damage libmechanical_damage.so "$2" || exit 2; sed -i "s/^effect_scale *=.*/effect_scale = $scale/" "$1/mechanical-damage/mod.toml"; }
  if [ $others -eq 1 ]; then
    for pair in analog-pedals:libanalog_pedals.so cheats:libcheats.so draw-distance:libdraw_distance.so orbit-camera:liborbit_camera.so reset-to-track:libreset_to_track.so \
                rollover:librollover.so deformation:libdeformation.so; do
      install_mod "$1" "${pair%%:*}" "${pair#*:}" true || { echo "mod_damage_check: cannot install ${pair%%:*}" >&2; exit 2; }
    done
  fi
}
declare -a extra_env=()
# run_one LABEL MODS_DIR FRAMES(0|1)
run_one() {
  local label=$1 mdir=$2 frames=$3
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 GT_MOD_DAMAGE_LOG=1
                 GT_PAD="$(grep -v '^#' "$route")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every)
  vars+=("${extra_env[@]}")
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  local -a lock=(tools/cpu_lock.sh "mod_damage_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do
    "${lock[@]}" env "${vars[@]}" systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?
    [ $rc -ne 143 ] && break
    echo "mod_damage_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  [ $rc -eq 0 ] || { echo "mod_damage_check: $label: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; }
  grep -aq 'script done' "$out/$label/run.log" || { echo "mod_damage_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; }
  grep -ah '^\[mods\]' "$out/$label/run.log" | head -3
}
py=.venv/bin/python
make_mods "$out/mods_off" false
make_mods "$out/mods" true
rc=0
judge() { $py -I tools/mod_damage_check.py "$out" "$1" || rc=1; }
cfg="decomp=$decomp seconds=$seconds others=$others route=$route"
reuse_ok() { [ -n "${KEEP_BASELINE:-}" ] && [ "$(cat "$out/.config" 2>/dev/null)" = "$cfg" ] && [ -d "$out/$1" ]; }
if has replay; then
  extra_env=(GT_MOD_DAMAGE_FAKE=m30:25:f)
  run_one replay "$out/mods" 0 || exit 2
  judge replay
  exit $rc
fi
if has toggle-off; then
  extra_env=(GT_MOD_DAMAGE_FAKE=m30:25:f GT_MOD_MENU_TEST_TOGGLE=mechanical-damage@v${TOGGLE_AT:-15800})
  run_one toggle "$out/mods" 0 || exit 2
  judge toggle
  exit $rc
fi
if has idle; then
  extra_env=()
  reuse_ok off || run_one off "$out/mods_off" 1 || exit 2
  run_one idle "$out/mods" 1 || exit 2
else
  extra_env=(); reuse_ok idle || run_one idle "$out/mods" 0 || exit 2   # the undamaged speed profile the other judges compare to
fi
echo "$cfg" > "$out/.config"
has idle && judge idle
if has power; then extra_env=(GT_MOD_DAMAGE_FAKE=m30:25:f); run_one front "$out/mods" 0 || exit 2; judge power; fi
if has repair; then extra_env=(GT_MOD_DAMAGE_FAKE=m30:25:f GT_MOD_DAMAGE_REPAIR=m200); run_one repair "$out/mods" 0 || exit 2; judge repair; fi
if has cap; then extra_env=(GT_MOD_DAMAGE_FAKE=m30:40:b); run_one rear "$out/mods" 0 || exit 2; judge cap; fi
if has pull; then
  extra_env=(GT_MOD_DAMAGE_FAKE=m30:40:r); run_one right "$out/mods" 0 || exit 2
  extra_env=(GT_MOD_DAMAGE_FAKE=m30:40:l); run_one left "$out/mods" 0 || exit 2
  judge pull
fi
[ $rc -eq 0 ] && echo "mod_damage_check: PASS" || echo "mod_damage_check: FAIL"
exit $rc
