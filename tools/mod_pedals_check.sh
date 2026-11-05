#!/usr/bin/env bash
# In-game check of the analog-pedals mod (gt-mods repo): headless gt_play on the `race` pad route with the mod installed
# and GT_MOD_PEDALS_FAKE=0.5,0.3 (throttle 0.5, brake 0.3 without touching a controller). The mod logs the player's and an
# AI car's pedals (GT_MOD_PEDALS_LOG=1); tools/mod_pedals_check.py passes when the player's pedals (car+8+0x530 accelerator,
# +0x538 brake) are 0.5 / 0.3 and the AI car's pedals differ from the player's.
#   tools/mod_pedals_check.sh [--without-mod] [--sabotage|--sabotage-brake] [--disabled] [--toggle-off [--toggle-at S] [--sabotage-active]] [--sdl] [--decomp off|on] [--seconds N] [--route NAME]
#   --without-mod  empty mods dir (the check must FAIL: nothing injects)
#   --sabotage     builds the mod with ANALOG_PEDALS_SABOTAGE=1 (throttle written to the wrong record field; must FAIL)
#   --sabotage-brake  same with =2 (brake written to the wrong field; must FAIL)
#   --disabled     mod installed with enabled = false: prints the `[mods] ... skipped` line and `script done at vblank N` (compare N
#                  with a --without-mod run: they must be equal); no pedal check
#   --toggle-off   mod menu check: the mod is enabled, and GT_MOD_MENU_TEST_TOGGLE=analog-pedals@<--toggle-at seconds, default 120> makes
#                  gt_play call the menu's own toggle mid-race (logs `[mods] analog-pedals -> OFF`). PASS = pedals injected before that
#                  line and no mod log line after it (an inactive mod's code never runs).
#   --sabotage-active  with --toggle-off, expects a build configured with -DGT_MOD_SABOTAGE_IGNORE_ACTIVE=ON (the trampoline ignores
#                  `active`): the check must FAIL. Reconfigure the build without the option afterwards.
#   --sdl          no GT_MOD_PEDALS_FAKE: the mod reads SDL (keyboard/controller) while gt_play pumps events, for a long run
#                  (--seconds 600): passes when gt_play exits 0 and the mod logged; checks no pedal values
# Env: MOD_CHECK_NO_LOCK=1 (the caller already holds tools/cpu_lock.sh), GT_MODS_REPO (default ../gt-mods), BUILD (default build). Exit: 0 pass, 1 check failed, 2 setup/run problem.
# Runs inside the memory cgroup and the shared CPU lock (a real-time run); output under $OUT (default
# recomp/analysis/mod_pedals_check, gitignored).
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo" || exit 2
mods_repo=${GT_MODS_REPO:-$repo/../gt-mods}
build=${BUILD:-build}
out=${OUT:-recomp/analysis/mod_pedals_check}
decomp=off seconds=240 route=race without=0 sabotage=0 disabled=0 sdl=0 toggle=0 toggle_at=120 sab_active=0
while [ $# -gt 0 ]; do
  case $1 in
    --without-mod) without=1;; --sabotage) sabotage=1;; --sabotage-brake) sabotage=2;; --disabled) disabled=1;; --sdl) sdl=1;; --toggle-off) toggle=1;; --toggle-at) toggle_at=$2; shift;; --sabotage-active) sab_active=1;;
    --decomp) decomp=$2; shift;; --seconds) seconds=$2; shift;; --route) route=$2; shift;;
    *) echo "mod_pedals_check: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
[ -x "$build/gt_play" ] || { echo "mod_pedals_check: no $build/gt_play" >&2; exit 2; }
if [ $sab_active -eq 1 ]; then
  [ $toggle -eq 1 ] || { echo "mod_pedals_check: --sabotage-active needs --toggle-off" >&2; exit 2; }
  grep -q '^GT_MOD_SABOTAGE_IGNORE_ACTIVE:BOOL=ON' "$build/CMakeCache.txt" 2>/dev/null \
    || { echo "mod_pedals_check: $build is not configured with -DGT_MOD_SABOTAGE_IGNORE_ACTIVE=ON (rebuild gt_play first)" >&2; exit 2; }
fi
[ -f "config/pad_routes/$route.pad" ] || { echo "mod_pedals_check: no pad route $route" >&2; exit 2; }
rm -rf "$out" && mkdir -p "$out/mods" "$out/savedata" || exit 2
# Routes that start from the user's save (save_*, perf_* like perf_tt_newyork: Time Trial) get a fresh copy of it, as in gpu_gate.sh.
case $route in save_*|perf_*) cp -r game_files/UCES01245-GAMEDAT "$out/savedata/" || exit 2 ;; esac
if [ $without -eq 0 ]; then
  mbuild="$out/mod-build"
  flags=""; [ $sabotage -ne 0 ] && flags="-DCMAKE_CXX_FLAGS=-DANALOG_PEDALS_SABOTAGE=$sabotage"
  { cmake -S "$mods_repo" -B "$mbuild" -DCMAKE_CXX_COMPILER=clang++ -DGT_SDK_DIR="$repo/include" $flags \
      && cmake --build "$mbuild" -j2 --target analog_pedals; } > "$out/mod-build.log" 2>&1 \
    || { echo "mod_pedals_check: mod build failed (log $out/mod-build.log)" >&2; exit 2; }
  mkdir -p "$out/mods/analog-pedals"
  cp "$mbuild/analog-pedals/libanalog_pedals.so" "$out/mods/analog-pedals/" || exit 2
  if [ $disabled -eq 1 ]; then cp "$mods_repo/analog-pedals/mod.toml" "$out/mods/analog-pedals/mod.toml"
  else sed 's/^enabled = false/enabled = true/' "$mods_repo/analog-pedals/mod.toml" > "$out/mods/analog-pedals/mod.toml"; fi
fi
pad=$(grep -v '^#' "config/pad_routes/$route.pad")
lock="tools/cpu_lock.sh mod_pedals_check_$route"; [ -n "${MOD_CHECK_NO_LOCK:-}" ] && lock=""
fake=0.5,0.3; [ $sdl -eq 1 ] && fake=
toggle_env=""; [ $toggle -eq 1 ] && toggle_env="analog-pedals@$toggle_at"
env GT_MOD_MENU_TEST_TOGGLE="$toggle_env" GT_MODS_DIR="$repo/$out/mods" GT_SAVEDATA_DIR="$repo/$out/savedata" SDL_VIDEODRIVER=dummy LCS_UNCAPPED=1 \
    GT_MOD_PEDALS_FAKE="$fake" GT_MOD_PEDALS_LOG=1 GT_PAD="$pad" GT_PAD_LOG=1 \
  $lock systemd-run --user --scope --quiet --slice=gt-heavy.slice -- \
  timeout $((seconds + 120)) "$build/gt_play" --max-seconds "$seconds" --decomp "$decomp" > "$out/run.log" 2>&1
rc=$?
if [ $rc -ne 0 ]; then echo "mod_pedals_check: gt_play exited $rc (log $out/run.log)" >&2; exit 2; fi
grep -h '^\[mods\]' "$out/run.log"
grep -h 'script done' "$out/run.log"
if [ $disabled -eq 1 ]; then grep -q '^\[mods\].*skipped' "$out/run.log" && exit 0; echo "mod_pedals_check: no [mods] skipped line" >&2; exit 1; fi
if [ $sdl -eq 1 ]; then grep -q '\[mod:analog-pedals\] step=' "$out/run.log" && { echo "PASS: ran $seconds s on the SDL path, exit 0"; exit 0; }; echo "FAIL: the mod logged nothing" >&2; exit 1; fi
if [ $toggle -eq 1 ]; then .venv/bin/python -I tools/mod_pedals_check.py "$out/run.log" --toggle; exit $?; fi
.venv/bin/python -I tools/mod_pedals_check.py "$out/run.log"
