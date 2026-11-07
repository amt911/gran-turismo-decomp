#!/usr/bin/env bash
# In-game check of the reset-to-track mod (gt-mods repo): headless gt_play on the `reset` pad route (the race, throttle held until the
# car leaves the road at the end of the straight and hits the barrier, then SELECT held, then the throttle again), the button given
# by GT_MOD_RESET_FAKE=<player-car step> (no controller needed) and the mod's state log (GT_MOD_RESET_LOG=1).
# tools/mod_reset_check.py judges: after the reset all four wheels on asphalt, up.z > 0.9, the car restarts from rest, keeps its
# heading and drives for 600 steps; with the mod installed but no button every frame is byte-identical to a run without the mod.
#   tools/mod_reset_check.sh [--decomp off|on] [--without-mod|--sabotage-rates|--sabotage-newest|--toggle-off|--reset-only] [--fake STEP] [--seconds N] [--every N]
#   (default)         runs: baseline (no mod, frames), quiet (mod, no button, frames), reset (mod + button, log); PASS only if both judge well
#   --without-mod     empty mods dir for the reset run (must FAIL: nothing resets)
#   --sabotage-rates  mod built with -DRESET_SABOTAGE=1: restore without zeroing the angular rates (must FAIL: the heading drifts)
#   --sabotage-newest mod built with -DRESET_SABOTAGE=2: restore the newest pose, off-track included (must FAIL: wheels off asphalt)
#   --reset-only      just the reset run (no baseline, no frames), judged by the restore criteria
#   --toggle-off      the F9 menu's toggle turns the mod off (GT_MOD_MENU_TEST_TOGGLE=reset-to-track@v<--toggle-at>, default v16300) before
#                     the button: PASS = `-> OFF` logged, no reset, frames identical to the baseline
# Env: ROUTE (a pad route file, default config/pad_routes/reset.pad), MOD_CHECK_NO_LOCK=1 (no lock: the caller holds tools/cpu_lock.sh), GT_MODS_REPO (default ../gt-mods), BUILD (default build),
# OUT (default recomp/analysis/mod_reset_check, gitignored; frames on disk there, never /tmp), KEEP_BASELINE=1 (reuse $OUT/baseline, same
# --decomp). Exit: 0 pass, 1 check failed, 2 setup/run problem. Each run is a real-time job: inside the memory cgroup and its own hold of the shared
# CPU lock (tools/cpu_lock.sh).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_reset_check}
decomp=off seconds=200 every=30 without=0 sabotage=0 toggle=0 reset_only=0 toggle_at=16300 fake=${GT_RESET_FAKE_STEP:-1851}
while [ $# -gt 0 ]; do
  case $1 in
    --without-mod) without=1;; --sabotage-rates) sabotage=1;; --sabotage-newest) sabotage=2;; --toggle-off) toggle=1;; --toggle-at) toggle_at=$2; shift;;
    --fake) fake=$2; shift;; --reset-only) reset_only=1;; --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --every) every=$2; shift;;
    *) echo "mod_reset_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ "$fake" = none ] && fake=-  # --fake none: the reset run without the button (debugging)
[ -x "$build/gt_play" ] || { echo "mod_reset_check: no $build/gt_play" >&2; exit 2; }
route=${ROUTE:-config/pad_routes/reset.pad}
[ -f "$route" ] || { echo "mod_reset_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out/nomods" || exit 2
mkdir -p "$out/mods/reset-to-track"
if [ $without -eq 0 ]; then
  mbuild="$out/mod-build-s$sabotage"
  flags=""; [ $sabotage -ne 0 ] && flags="-DRESET_SABOTAGE=$sabotage"
  { cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" \
      && cmake --build "$mbuild" -j2 --target reset_to_track; } > "$out/mod-build-s$sabotage.log" 2>&1 \
    || { echo "mod_reset_check: mod build failed (log $out/mod-build-s$sabotage.log)" >&2; exit 2; }
  cp "$mbuild/reset-to-track/libreset_to_track.so" "$out/mods/reset-to-track/" || exit 2
  sed 's/^enabled = false/enabled = true/' "$mods_repo/reset-to-track/mod.toml" > "$out/mods/reset-to-track/mod.toml"
fi
pad=$(grep -v '^#' "$route")
# run_one LABEL MODS_DIR FAKE_STEP|- TOGGLE_SPEC|- FRAMES(0|1)
run_one() {
  local label=$1 mdir=$2 fakestep=$3 tog=$4 frames=$5
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  # only the variables that are set: the host reads an empty GT_FRAME_DUMP as a path and aborts
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1
                 GT_MOD_RESET_LOG=1 GT_PAD="$pad" GT_PAD_LOG=1 GT_FRAME_EVERY=$every)
  [ "$fakestep" != "-" ] && vars+=(GT_MOD_RESET_FAKE="$fakestep")
  [ "$tog" != "-" ] && vars+=(GT_MOD_MENU_TEST_TOGGLE="$tog")
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  # one lock hold per run (a hold is killed when another session's wait passes 30 min; the run is then repeated, up to 4 times)
  local -a lock=(tools/cpu_lock.sh "mod_reset_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try
  for try in 1 2 3 4; do
    "${lock[@]}" env "${vars[@]}" \
      systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    [ $? -ne 143 ] && break
    echo "mod_reset_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  local rc=$?
  if [ $rc -ne 0 ]; then echo "mod_reset_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  cp "$out/$label/run.log" "$out/$label-s$sabotage-$decomp.log"  # kept: the next run of the same label removes $out/$label
  grep -h '^\[mods\]' "$out/$label/run.log"; grep -h 'script done' "$out/$label/run.log"
}
mdir="$out/mods"; [ $without -eq 1 ] && mdir="$out/nomods"
py=.venv/bin/python
if [ $toggle -eq 1 ]; then
  if [ -z "${KEEP_BASELINE:-}" ] || [ ! -d "$out/baseline/frames" ]; then run_one baseline "$out/nomods" - - 1 || exit 2; fi
  run_one toggle "$mdir" "$fake" "reset-to-track@v$toggle_at" 1 || exit 2
  $py -I tools/mod_reset_check.py "$out" toggle
  exit $?
fi
if [ $sabotage -ne 0 ] || [ $without -eq 1 ] || [ $reset_only -eq 1 ]; then  # a failing run is all that is asked of a sabotage: no baseline needed
  run_one reset "$mdir" "$fake" - 0 || exit 2
  $py -I tools/mod_reset_check.py "$out" restore
  exit $?
fi
if [ -z "${KEEP_BASELINE:-}" ] || [ ! -d "$out/baseline/frames" ]; then run_one baseline "$out/nomods" - - 1 || exit 2; fi
run_one quiet "$mdir" - - 1 || exit 2
run_one reset "$mdir" "$fake" - 0 || exit 2
rc=0
$py -I tools/mod_reset_check.py "$out" quiet || rc=1
$py -I tools/mod_reset_check.py "$out" restore || rc=1
exit $rc
