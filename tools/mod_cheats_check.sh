#!/usr/bin/env bash
# In-game check of the cheats mod (gt-mods repo): headless gt_play on the `cheats` pad route (the race, throttle held down the straight), the
# mod's car log (GT_MOD_CHEATS_LOG=1) and tools/mod_cheats_check.py as judge.
#   tools/mod_cheats_check.sh [--decomp off|on] [--seconds N] [--pad] [--keep-baseline]
#                             [--sabotage-slowmo|--sabotage-freeze|--sabotage-chord]
#   (default)    baseline (no mod, frames), disabled (mod installed with enabled = false: frames byte-identical), idle (mod on, every cheat off: frames
#                byte-identical, and its log is the reference trajectory), slow05 and slow025 (GT_MOD_CHEATS_SLOWMO: the same physics at 0.5 / 0.25 of
#                the clock, AI cars slowed too), freeze (GT_MOD_CHEATS_FREEZE_AI=1 from player-car step 860, once the AI cars are down the straight: still while the player drives); PASS only if all pass
#   --pad        the `cheats_pad` route: the pad chord SELECT+START opens the F9 menu, Down/Right set Slow motion 0.5x and Freeze AI on, the chord
#                closes it; PASS = the menu keys arrive, the game sees no throttle while it is open, the chosen cheats then act
#   Each of these must FAIL (exit 1): --sabotage-slowmo (-DCHEATS_SABOTAGE=1: slow motion does nothing), --sabotage-freeze (=2: freeze does
#   nothing), --sabotage-chord (rebuilds gt_play with -DGT_PAD_MENU_SABOTAGE_IGNORE_CHORD=ON and runs --pad: the chord never opens the menu; the
#   normal gt_play is rebuilt afterwards)
# Env: GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_cheats_check, gitignored; never /tmp),
# MOD_CHECK_NO_LOCK=1 (the caller holds tools/cpu_lock.sh), KEEP_BASELINE=1 or --keep-baseline (reuse the baseline of $OUT, same --decomp).
# Exit: 0 pass, 1 check failed, 2 setup/run problem (a gt_play that crashed, timed out or did not finish its route). Each run is a real-time
# job: inside the memory cgroup and its own hold of the shared CPU lock (tools/cpu_lock.sh).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_cheats_check}
decomp=off seconds=170 pad=0 sabotage=0 chord_sabotage=0
[ -n "${KEEP_BASELINE:-}" ] && keep=1 || keep=0
while [ $# -gt 0 ]; do
  case $1 in
    --pad) pad=1;; --keep-baseline) keep=1;; --sabotage-slowmo) sabotage=1;; --sabotage-freeze) sabotage=2;; --sabotage-chord) chord_sabotage=1; pad=1;;
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;;
    *) echo "mod_cheats_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_cheats_check: no $build/gt_play" >&2; exit 2; }
route=config/pad_routes/cheats.pad; [ $pad -eq 1 ] && route=config/pad_routes/cheats_pad.pad
[ -f "$route" ] || { echo "mod_cheats_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out/nomods" "$out/mods/cheats" "$out/mods_disabled/cheats" || exit 2
mbuild="$out/mod-build-s$sabotage"
flags=""; [ $sabotage -ne 0 ] && flags="-DCHEATS_SABOTAGE=$sabotage"
{ cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" \
    && cmake --build "$mbuild" -j2 --target cheats; } > "$out/mod-build-s$sabotage.log" 2>&1 \
  || { echo "mod_cheats_check: mod build failed (log $out/mod-build-s$sabotage.log)" >&2; exit 2; }
cp "$mbuild/cheats/libcheats.so" "$out/mods/cheats/" && cp "$mbuild/cheats/libcheats.so" "$out/mods_disabled/cheats/" || exit 2
sed 's/^enabled = false/enabled = true/' "$mods_repo/cheats/mod.toml" > "$out/mods/cheats/mod.toml"
cp "$mods_repo/cheats/mod.toml" "$out/mods_disabled/cheats/mod.toml"   # shipped state: enabled = false

# run_one LABEL MODS_DIR FRAMES(0|1) ROUTE_FILE [VAR=VALUE ...]
run_one() {
  local label=$1 mdir=$2 frames=$3 rfile=$4; shift 4
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  # only the variables that are set: the host reads an empty GT_FRAME_DUMP as a path and aborts
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1
                 GT_MOD_CHEATS_LOG=1 GT_MOD_CHEATS_LOG_EVERY=2 GT_PAD="$(grep -v '^#' "$rfile")" GT_PAD_LOG=1 GT_PAD_EXIT_AFTER=120 GT_FRAME_EVERY=30 "$@")
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  local -a lock=(tools/cpu_lock.sh "mod_cheats_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do  # a hold is killed when another session's wait passes 30 min; the run is then repeated, up to 4 times
    "${lock[@]}" env "${vars[@]}" \
      systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?  # gt_play's own status, taken right here: a crash (134), a timeout (124) or a stopped hold must not pass for a run
    [ $rc -ne 143 ] && break
    echo "mod_cheats_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  if [ $rc -ne 0 ]; then echo "mod_cheats_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  if ! grep -aq 'script done' "$out/$label/run.log"; then echo "mod_cheats_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; fi
  cp "$out/$label/run.log" "$out/$label-s$sabotage-$decomp.log"  # kept: the next run of the same label removes $out/$label
  grep -ah '^\[mods\]' "$out/$label/run.log"; grep -ah 'script done' "$out/$label/run.log"
}
py=.venv/bin/python
judge() { $py -I tools/mod_cheats_check.py "$out" "$@"; }

if [ $pad -eq 1 ]; then
  if [ $chord_sabotage -eq 1 ]; then  # a gt_play whose chord never fires; rebuilt as it was afterwards
    restore() { cmake -S . -B "$build" -DGT_PAD_MENU_SABOTAGE_IGNORE_CHORD=OFF > /dev/null 2>&1 && cmake --build "$build" -j4 --target gt_play > /dev/null 2>&1; }
    trap restore EXIT
    { cmake -S . -B "$build" -DGT_PAD_MENU_SABOTAGE_IGNORE_CHORD=ON && cmake --build "$build" -j4 --target gt_play; } > "$out/gt_play-sabotage-build.log" 2>&1 \
      || { echo "mod_cheats_check: sabotage build failed (log $out/gt_play-sabotage-build.log)" >&2; exit 2; }
  fi
  run_one pad "$out/mods" 0 "$route" || exit 2
  judge pad pad
  exit $?
fi
# default: identity, slow motion, freeze. keep=1 reuses the baseline frames of $out (same --decomp, recorded in baseline/.config)
cfg="decomp=$decomp route=$route seconds=$seconds"
if [ $keep -eq 0 ] || [ ! -d "$out/baseline/frames" ] || [ "$(cat "$out/baseline/.config" 2>/dev/null)" != "$cfg" ]; then
  run_one baseline "$out/nomods" 1 "$route" || exit 2
  echo "$cfg" > "$out/baseline/.config"
fi
rc=0
if [ $sabotage -eq 0 ]; then
  run_one disabled "$out/mods_disabled" 1 "$route" || exit 2
  judge identity disabled || rc=1
fi
run_one idle "$out/mods" $((sabotage == 0)) "$route" || exit 2
[ $sabotage -eq 0 ] && { judge identity idle || rc=1; }
if [ $sabotage -ne 2 ]; then
  run_one slow05 "$out/mods" 0 "$route" GT_MOD_CHEATS_SLOWMO=0.5 || exit 2
  judge slowmo slow05 0.5 || rc=1
  if [ $sabotage -eq 0 ]; then
    run_one slow025 "$out/mods" 0 "$route" GT_MOD_CHEATS_SLOWMO=0.25 || exit 2
    judge slowmo slow025 0.25 || rc=1
  fi
fi
if [ $sabotage -ne 1 ]; then
  run_one freeze "$out/mods" 0 "$route" GT_MOD_CHEATS_FREEZE_AI=1 GT_MOD_CHEATS_FROM=${FREEZE_FROM:-860} || exit 2
  judge freeze freeze || rc=1
fi
exit $rc
