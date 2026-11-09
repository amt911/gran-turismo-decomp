#!/usr/bin/env bash
# In-game check of the rollover mod (gt-mods repo): headless gt_play on the `rollover` pad route (the race, throttle held until the car runs off
# the end of the straight and hits the barrier at a slant, then throttle and brake), the mod's state log (GT_MOD_ROLLOVER_LOG=1) and
# tools/mod_rollover_check.py as judge.
#   tools/mod_rollover_check.sh [--decomp off|on] [--seconds N (default 150: the route ends at vblank ~17980, ~110 s; see docs/FINDINGS.md for why not longer)] [--every N] [--roll-only] [--toggle-off] [--recover]
#                               [--without-mod|--sabotage-noflag|--sabotage-noimpulse|--sabotage-threshold0|--sabotage-flipflag|--sabotage-nobox]
#   (default)    baseline (no mod, frames), disabled (the mod installed with enabled = false: frames byte-identical to the baseline, nothing
#                logged), roll (mod enabled: quiet driving first, one impulse at the crash, the car tilts past the game's 35 deg clamp toward
#                the side it hit, the dormant flip machine stays asleep, no height jump, ends upright or upside down at rest); PASS only if all pass
#   (the roll run also judges the body box, mod 0.2.0: `BOX` lines: the deepest box corner of the tumbling car above -0.25 m, the origin not launched past 0.8 m, no
#                touch while upright; and the default flow adds a `nobox` run, the mod with body_box = false: the state lines before the first impact must be
#                byte-identical with and without the box, and without it the body must sink > 0.5 m)
#   --frames     one more run with the box on and a frame every 3 vblanks, pruned to the vblanks around the crash (--frames-window FROM:TO, default auto: 60 vblanks before the first impulse to 300 after): PPMs in $OUT/tumble/frames (tumble_nobox with --without-box) for a person to look at (no judge); with --without-box the same for the box off
#   --roll-only  just the roll run (no baseline, no disabled run)
#   --toggle-off the F9 menu's toggle turns the mod off (GT_MOD_MENU_TEST_TOGGLE=rollover@v<--toggle-at>, default v16300) before the crash:
#                PASS = the mod ran before `-> OFF` and logged nothing after, the frames from 120 vblanks after the toggle to the route's end
#                (the crash included: it does not roll) identical to the baseline
#   --recover    rollover + reset-to-track enabled together on the `rollover_reset` route (SELECT held so that the reset fires ~20 steps after
#                the crash): the car is upside down, still tumbling, when the reset fires; PASS = rolled before the RESTORE line (rollover judge) and the reset judge (tools/mod_reset_check.py restore) passes
#   Each of these must FAIL: --without-mod (-DROLLOVER_SABOTAGE=5: stock physics, the mod only logs: nothing rolls; with --recover the car is not upside down at the reset), --sabotage-noflag (-DROLLOVER_SABOTAGE=1: the impulse without the flag, the game's
#   clamp holds the tilt), --sabotage-noimpulse (=2: the flag without any impulse: the barrier alone does not roll the car),
#   --sabotage-threshold0 (=3: an impulse in normal driving), --sabotage-flipflag (=4: the flag also lifted for the dormant flip state machine: the car sinks through the road),
#   --sabotage-nobox (=6: body_box off, the log still measures: the body sinks 1.3 m, the box judge fails on the depth)
# Env: ROUTE (a pad route file, default config/pad_routes/rollover.pad), MOD_CHECK_NO_LOCK=1 (no lock: the caller holds tools/cpu_lock.sh),
# GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_rollover_check, gitignored; never /tmp),
# KEEP_BASELINE=1 (reuse the baseline of $OUT when --decomp, route, --every and --seconds match). Exit: 0 pass, 1 check failed, 2 setup/run problem (a gt_play that crashed, timed out
# or did not finish its route). Each run is a real-time job: inside the memory cgroup and its own hold of the shared CPU lock (tools/cpu_lock.sh).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_rollover_check}
decomp=off seconds=150 every=30 sabotage=0 toggle=0 roll_only=0 recover=0 toggle_at=16300 frames=0 fwindow=auto nobox_frames=0
while [ $# -gt 0 ]; do
  case $1 in
    --without-mod) sabotage=5;; --sabotage-noflag) sabotage=1;; --sabotage-noimpulse) sabotage=2;; --sabotage-threshold0) sabotage=3;; --sabotage-flipflag) sabotage=4;; --sabotage-nobox) sabotage=6;; --frames) frames=1;; --frames-window) fwindow=$2; shift;; --without-box) nobox_frames=1;;
    --toggle-off) toggle=1;; --toggle-at) toggle_at=$2; shift;; --roll-only) roll_only=1;; --recover) recover=1;;
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --every) every=$2; shift;;
    *) echo "mod_rollover_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_rollover_check: no $build/gt_play" >&2; exit 2; }
route=${ROUTE:-config/pad_routes/rollover.pad}
[ $recover -eq 1 ] && route=${ROUTE:-config/pad_routes/rollover_reset.pad}
[ -f "$route" ] || { echo "mod_rollover_check: no pad route $route" >&2; exit 2; }
mkdir -p "$out/nomods" "$out/mods/rollover" "$out/mods_disabled/rollover" "$out/mods_recover/rollover" "$out/mods_recover/reset-to-track" "$out/mods_nobox/rollover" || exit 2
if true; then
  mbuild="$out/mod-build-s$sabotage"
  flags=""; [ $sabotage -ne 0 ] && flags="-DROLLOVER_SABOTAGE=$sabotage"
  { cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" "-DCMAKE_CXX_FLAGS=$flags" \
      && cmake --build "$mbuild" -j2 --target rollover reset_to_track; } > "$out/mod-build-s$sabotage.log" 2>&1 \
    || { echo "mod_rollover_check: mod build failed (log $out/mod-build-s$sabotage.log)" >&2; exit 2; }
  cp "$mbuild/rollover/librollover.so" "$out/mods/rollover/" && cp "$mbuild/rollover/librollover.so" "$out/mods_disabled/rollover/" \
    && cp "$mbuild/rollover/librollover.so" "$out/mods_recover/rollover/" && cp "$mbuild/reset-to-track/libreset_to_track.so" "$out/mods_recover/reset-to-track/" || exit 2
  sed 's/^enabled = false/enabled = true/' "$mods_repo/rollover/mod.toml" > "$out/mods/rollover/mod.toml"
  cp "$mods_repo/rollover/mod.toml" "$out/mods_disabled/rollover/mod.toml"   # shipped state: enabled = false
  cp "$out/mods/rollover/mod.toml" "$out/mods_recover/rollover/mod.toml"
  cp "$mbuild/rollover/librollover.so" "$out/mods_nobox/rollover/" && sed 's/^body_box *=.*/body_box = false/' "$out/mods/rollover/mod.toml" > "$out/mods_nobox/rollover/mod.toml"
  # rewind_ms 4500 as the reset check does: the restored pose is ~130 m before the barrier
  sed 's/^enabled = false/enabled = true/; s/^rewind_ms *=.*/rewind_ms = 4500/' "$mods_repo/reset-to-track/mod.toml" > "$out/mods_recover/reset-to-track/mod.toml"
fi

# run_one LABEL MODS_DIR TOGGLE_SPEC|- FRAMES(0|1) ROUTE_FILE
run_one() {
  local label=$1 mdir=$2 tog=$3 frames=$4 rfile=$5
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  # only the variables that are set: the host reads an empty GT_FRAME_DUMP as a path and aborts
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1
                 GT_MOD_ROLLOVER_LOG=1 GT_MOD_RESET_LOG=1 GT_PAD="$(grep -v '^#' "$rfile")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every)
  [ "$tog" != "-" ] && vars+=(GT_MOD_MENU_TEST_TOGGLE="$tog")
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  local -a lock=(tools/cpu_lock.sh "mod_rollover_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do  # a hold is killed when another session's wait passes 30 min; the run is then repeated, up to 4 times
    "${lock[@]}" env "${vars[@]}" \
      systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?  # gt_play's own status, taken right here: a crash (134), a timeout (124) or a stopped hold must not pass for a run
    [ $rc -ne 143 ] && break
    echo "mod_rollover_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  if [ $rc -ne 0 ]; then echo "mod_rollover_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  if ! grep -aq 'script done' "$out/$label/run.log"; then echo "mod_rollover_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; fi
  cp "$out/$label/run.log" "$out/$label-s$sabotage-$decomp.log"  # kept: the next run of the same label removes $out/$label
  grep -ah '^\[mods\]' "$out/$label/run.log"; grep -ah 'script done' "$out/$label/run.log"
}
mdir="$out/mods"
py=.venv/bin/python
# KEEP_BASELINE=1 reuses $out/baseline only when it was made with the same --decomp, route, --every and --seconds (recorded in baseline/.config)
cfg="decomp=$decomp route=$route every=$every seconds=$seconds"
need_baseline() { [ -z "${KEEP_BASELINE:-}" ] || [ ! -d "$out/baseline/frames" ] || [ "$(cat "$out/baseline/.config" 2>/dev/null)" != "$cfg" ]; }
make_baseline() { run_one baseline "$out/nomods" - 1 "$1" && echo "$cfg" > "$out/baseline/.config"; }
if [ $recover -eq 1 ]; then
  run_one recover "$out/mods_recover" - 0 "$route" || exit 2
  rc=0
  $py -I tools/mod_rollover_check.py "$out" recover || rc=1
  mkdir -p "$out/rr" && ln -sfn ../recover "$out/rr/reset"   # the reset judge reads <dir>/reset/run.log
  # calm window -1: the reset judge's "rates are zero right after the restore" is not asked here (a car reset out of a tumble wobbles on its
  # springs for ~15 steps, ~1 rad/s at the first logged step in --decomp on): the rollover judge asks that the roll rate has settled by +20
  $py -I tools/mod_reset_check.py "$out/rr" restore -1 || rc=1
  exit $rc
fi
if [ $toggle -eq 1 ]; then
  need_baseline && { make_baseline "$route" || exit 2; }
  run_one toggle "$mdir" "rollover@v$toggle_at" 1 "$route" || exit 2
  $py -I tools/mod_rollover_check.py "$out" toggle "$toggle_at"
  exit $?
fi
if [ $frames -eq 1 ]; then  # the tumble in pictures, for a person: the box on (or off with --without-box), a frame every 3 vblanks around the crash
  fm="$out/mods" tl=tumble; [ $nobox_frames -eq 1 ] && { fm="$out/mods_nobox"; tl=tumble_nobox; }
  seconds=$((seconds * 3))  # a frame every 3 vblanks makes the run several times slower
  every=3 run_one $tl "$fm" - 1 "$route" || exit 2
  $py -I - "$out/$tl" "$fwindow" <<'PY'
import os, re, sys
d, window = sys.argv[1], sys.argv[2]
log = open(os.path.join(d, "run.log"), errors="replace").read()
if window == "auto":  # from 60 vblanks before the hit to 300 after: the vblank of the last frame dumped before the first IMPULSE line
    hit = re.search(r"IMPULSE st=", log)
    before = re.findall(r"\[frame-dump\] frame \d+ vblank (\d+)", log[:hit.start()]) if hit else []
    if not before:
        sys.exit("tumble: no IMPULSE line: the car never hit the barrier")
    lo, hi = int(before[-1]) - 60, int(before[-1]) + 300
else:
    lo, hi = (int(x) for x in window.split(":"))
keep = {int(f) for f, v in re.findall(r"\[frame-dump\] frame (\d+) vblank (\d+)", log) if lo <= int(v) <= hi}
n = 0
for name in os.listdir(os.path.join(d, "frames")):
    m = re.search(r"frame_(\d+)\.ppm$", name)
    if m and int(m.group(1)) not in keep:
        os.remove(os.path.join(d, "frames", name))
    elif m:
        n += 1
print(f"tumble: kept {n} frames of vblanks {lo}..{hi} in {d}/frames")
PY
  exit $?
fi
if [ $sabotage -ne 0 ] || [ $roll_only -eq 1 ]; then  # a failing run is all that is asked of a sabotage: no baseline needed
  run_one roll "$mdir" - 0 "$route" || exit 2
  $py -I tools/mod_rollover_check.py "$out" roll
  exit $?
fi
need_baseline && { make_baseline "$route" || exit 2; }
run_one disabled "$out/mods_disabled" - 1 "$route" || exit 2
run_one roll "$mdir" - 0 "$route" || exit 2
run_one nobox "$out/mods_nobox" - 0 "$route" || exit 2
rc=0
$py -I tools/mod_rollover_check.py "$out" disabled || rc=1
$py -I tools/mod_rollover_check.py "$out" roll || rc=1
$py -I tools/mod_rollover_check.py "$out" boxcmp || rc=1
exit $rc
