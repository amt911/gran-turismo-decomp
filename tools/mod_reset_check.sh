#!/usr/bin/env bash
# In-game check of the reset-to-track mod (gt-mods repo): headless gt_play on the `reset` pad route (the race, throttle held until the
# car leaves the road at the end of the straight and hits the barrier, then SELECT held 80 vblanks, then throttle and brake), the mod's
# state log (GT_MOD_RESET_LOG=1) and tools/mod_reset_check.py as judge. The SELECT hold is real: the mod reads the game's own pad buffer.
#   tools/mod_reset_check.sh [--decomp off|on] [--fake STEP] [--seconds N] [--every N] [--reset-only] [--tap] [--toggle-off]
#                            [--without-mod|--sabotage-rates|--sabotage-newest|--sabotage-norewind]
#   (default)    baseline_nosel (no mod, route without the SELECT hold), quiet (mod, same route: frames byte-identical, the mod ran), reset (mod,
#                full route: 4/4 wheels on asphalt, up.z > 0.9, restart from rest, calm, heading kept, >= 15 m/s with the throttle held,
#                rewound >= 1.5 s of travel, Select hidden from the game with no tap); PASS only if both judge well
#   --tap        the chase route's three short SELECT presses with and without the mod: replayed as taps, same camera view afterwards
#   --toggle-off the F9 menu's toggle turns the mod off (GT_MOD_MENU_TEST_TOGGLE=reset-to-track@v<--toggle-at>, default v16300) before the
#                press: PASS = the mod ran before `-> OFF` and logged nothing after, no reset, frames identical to the baseline
#   --reset-only just the reset run (no baseline), judged by the restore criteria
#   Each of these must FAIL (--sabotage-notap with --tap): --without-mod (nothing resets), --sabotage-rates (-DRESET_SABOTAGE=1: rates not zeroed), --sabotage-newest
#   (=2: newest pose, off-track included), --sabotage-norewind (=3: newest good pose, no rewind), --sabotage-notap (=4: the short
#   press is not replayed to the game)
# WITH_CHEATS=1 also installs the cheats mod (enabled): the pad menu is then active, so a lone Select reaches reset-to-track 8 vblanks late (use with --reset-only and --tap; the quiet run's
# byte-identity does not hold with the delay).
# Env: ROUTE (a pad route file, default config/pad_routes/reset.pad), MOD_CHECK_NO_LOCK=1 (no lock: the caller holds tools/cpu_lock.sh),
# GT_MODS_REPO (default ../gt-mods), BUILD (default build), OUT (default recomp/analysis/mod_reset_check, gitignored; frames on disk there,
# never /tmp), KEEP_BASELINE=1 (reuse the baseline of $OUT, same --decomp). Exit: 0 pass, 1 check failed, 2 setup/run problem (a gt_play that
# crashed, timed out or did not finish its route). Each run is a real-time job: inside the memory cgroup and its own hold of the shared
# CPU lock (tools/cpu_lock.sh).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_reset_check}
decomp=off seconds=200 every=30 without=0 sabotage=0 toggle=0 reset_only=0 tap=0 toggle_at=16300 fake=${GT_RESET_FAKE_STEP:-none}
while [ $# -gt 0 ]; do
  case $1 in
    --without-mod) without=1;; --sabotage-rates) sabotage=1;; --sabotage-newest) sabotage=2;; --sabotage-norewind) sabotage=3;; --sabotage-notap) sabotage=4;; --tap) tap=1;; --toggle-off) toggle=1;; --toggle-at) toggle_at=$2; shift;;
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
      && cmake --build "$mbuild" -j2 --target reset_to_track ${WITH_CHEATS:+cheats}; } > "$out/mod-build-s$sabotage.log" 2>&1 \
    || { echo "mod_reset_check: mod build failed (log $out/mod-build-s$sabotage.log)" >&2; exit 2; }
  cp "$mbuild/reset-to-track/libreset_to_track.so" "$out/mods/reset-to-track/" || exit 2
  # rewind_ms 4500 (the default is 2500): the restored car is ~130 m before the barrier, room to reach 15 m/s and stop
  if [ -n "${WITH_CHEATS:-}" ]; then  # an API-2 mod installed too: the host's pad menu (and its 8-vblank hold-back of a lone Select/Start) is then active
    mkdir -p "$out/mods/cheats" && cp "$mbuild/cheats/libcheats.so" "$out/mods/cheats/" && sed 's/^enabled = false/enabled = true/' "$mods_repo/cheats/mod.toml" > "$out/mods/cheats/mod.toml" || exit 2
  fi
  sed 's/^enabled = false/enabled = true/; s/^rewind_ms *=.*/rewind_ms = 4500/' "$mods_repo/reset-to-track/mod.toml" > "$out/mods/reset-to-track/mod.toml"
fi
# the same route without the SELECT hold (the quiet run: no button), and the camera-tap route
sed -E 's/,[0-9]+:SELECT:[0-9]+//' "$route" > "$out/route_nosel.pad"

# run_one LABEL MODS_DIR FAKE_STEP|- TOGGLE_SPEC|- FRAMES(0|1) ROUTE_FILE
run_one() {
  local label=$1 mdir=$2 fakestep=$3 tog=$4 frames=$5 rfile=$6
  rm -rf "$out/$label" && mkdir -p "$out/$label/frames" "$out/$label/savedata" || return 2
  # only the variables that are set: the host reads an empty GT_FRAME_DUMP as a path and aborts
  local -a vars=(GT_MODS_DIR="$repo/$mdir" GT_SAVEDATA_DIR="$repo/$out/$label/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1
                 GT_MOD_RESET_LOG=1 GT_PAD="$(grep -v '^#' "$rfile")" GT_PAD_LOG=1 GT_FRAME_EVERY=$every)
  [ "$fakestep" != "-" ] && vars+=(GT_MOD_RESET_FAKE="$fakestep")
  [ "$tog" != "-" ] && vars+=(GT_MOD_MENU_TEST_TOGGLE="$tog")
  [ "$frames" = 1 ] && vars+=(GT_FRAME_DUMP="$repo/$out/$label/frames")
  # one lock hold per run (a hold is killed when another session's wait passes 30 min; the run is then repeated, up to 4 times)
  local -a lock=(tools/cpu_lock.sh "mod_reset_check:$label"); [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=()
  local try rc=0
  for try in 1 2 3 4; do
    "${lock[@]}" env "${vars[@]}" \
      systemd-run --user --scope --quiet --slice=gt-heavy.slice -p CPUWeight=10000 -- \
      timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/$label/run.log" 2>&1
    rc=$?  # gt_play's own status, taken right here: a crash (134), a timeout (124) or a stopped hold must not pass for a run
    [ $rc -ne 143 ] && break
    echo "mod_reset_check: $label: the lock hold was stopped (rc 143), run $try of 4" >&2
  done
  if [ $rc -ne 0 ]; then echo "mod_reset_check: gt_play exited $rc (log $out/$label/run.log)" >&2; return 2; fi
  if ! grep -q 'script done' "$out/$label/run.log"; then echo "mod_reset_check: $label: the pad route did not finish (log $out/$label/run.log)" >&2; return 2; fi
  cp "$out/$label/run.log" "$out/$label-s$sabotage-$decomp.log"  # kept: the next run of the same label removes $out/$label
  grep -h '^\[mods\]' "$out/$label/run.log"; grep -h 'script done' "$out/$label/run.log"
}
mdir="$out/mods"; [ $without -eq 1 ] && mdir="$out/nomods"
py=.venv/bin/python
need_baseline() { [ -z "${KEEP_BASELINE:-}" ] || [ ! -d "$out/$1/frames" ]; }
if [ $tap -eq 1 ]; then  # the chase route's three short SELECT presses: hidden while down, replayed as taps, the camera view reached is the same
  need_baseline baseline_chase && { run_one baseline_chase "$out/nomods" - - 1 config/pad_routes/chase.pad || exit 2; }
  run_one tap "$mdir" - - 1 config/pad_routes/chase.pad || exit 2
  $py -I tools/mod_reset_check.py "$out" tap
  exit $?
fi
if [ $toggle -eq 1 ]; then
  need_baseline baseline && { run_one baseline "$out/nomods" - - 1 "$route" || exit 2; }
  run_one toggle "$mdir" "$fake" "reset-to-track@v$toggle_at" 1 "$route" || exit 2
  $py -I tools/mod_reset_check.py "$out" toggle
  exit $?
fi
if [ $sabotage -ne 0 ] || [ $without -eq 1 ] || [ $reset_only -eq 1 ]; then  # a failing run is all that is asked of a sabotage: no baseline needed
  run_one reset "$mdir" "$fake" - 0 "$route" || exit 2
  $py -I tools/mod_reset_check.py "$out" restore
  exit $?
fi
need_baseline baseline_nosel && { run_one baseline_nosel "$out/nomods" - - 1 "$out/route_nosel.pad" || exit 2; }
run_one quiet "$mdir" - - 1 "$out/route_nosel.pad" || exit 2
run_one reset "$mdir" "$fake" - 1 "$route" || exit 2
rc=0
$py -I tools/mod_reset_check.py "$out" quiet || rc=1
$py -I tools/mod_reset_check.py "$out" restore || rc=1
exit $rc
