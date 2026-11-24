#!/usr/bin/env bash
# The PR verdict pass (docs/agents/pr-verification.md) on a clean checkout of a pushed branch: setup, extract, recomp,
# a clang build, pytest with coverage, ctest, the full differential gate, the boot check and the synthetic gate after
# every route some function was verified after. Prints one line per step; the logs stay under recomp/analysis/.
#   tools/verdict.sh <branch> <tag>        (GT_VERIFY_TREE: the worktree to use, default ../gt-verify)
# GT_GATE_CPUS / GT_SYNTH_CPUS: the CPUs of the real-time gate and of the synthetic groups (taskset lists). The defaults split
# the 12-core/24-thread desktop in two halves of whole cores so two verdicts can overlap; a machine that runs one verdict at a
# time with fewer cores gives both all its CPUs (the helper laptop: 0-11): its gate on half the defaults' cores ran past the
# CPU lock's 30-min hold and lost five routes (verdict L575, 2026-11-12).
# Kept in the repo: a scratch copy was wiped by a reboot once (FINDINGS / lessons).
set -u
branch=$1 tag=$2
gate_cpus=${GT_GATE_CPUS:-6-11,18-23} synth_cpus=${GT_SYNTH_CPUS:-0-5,12-17}
tree=${GT_VERIFY_TREE:-$(dirname "$(git rev-parse --show-toplevel)")/gt-verify}
cd "$tree" || exit 1
# Worktrees share one ref store: two verdicts fetching at once can lose the ref lock ("cannot lock ref", verdict 615) — retry.
fetched=false
for try in 1 2 3 4 5; do git fetch -q origin && { fetched=true; break; }; sleep "${GT_FETCH_RETRY_S:-20}"; done
$fetched || { echo "fetch failed"; exit 1; }  # a stale origin/<branch> would verdict an old head
git checkout -q --detach "origin/$branch" || { echo "no branch $branch"; exit 1; }
echo "== head $(git rev-parse --short HEAD)"
logs=recomp/analysis/verdict_$tag
mkdir -p "$logs"
heavy() { systemd-run --user --scope --quiet --slice=gt-heavy.slice -- "$@"; }
tools/setup.sh > "$logs/setup.log" 2>&1 || { echo "setup failed"; exit 1; }
.venv/bin/python -m tools.extract --full > "$logs/extract.log" 2>&1 || { echo "extract failed"; exit 1; }
heavy .venv/bin/python -m tools.recomp > "$logs/recomp.log" 2>&1 || { echo "recomp failed"; tail -3 "$logs/recomp.log"; exit 1; }
# ccache (tools/setup.sh) only skips compiling units whose preprocessed content is identical: same objects as a clean build.
launcher=""; [ -x tools/src/ccache/ccache-4.10.2-linux-x86_64/ccache ] && launcher="-DCMAKE_CXX_COMPILER_LAUNCHER=$PWD/tools/src/ccache/ccache-4.10.2-linux-x86_64/ccache"
export CCACHE_DIR=${CCACHE_DIR:-$HOME/.cache/gt-verdict-ccache} CCACHE_MAXSIZE=${CCACHE_MAXSIZE:-20G}
heavy sh -c "cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ $launcher > /dev/null && cmake --build build -j8" \
  > "$logs/build.log" 2>&1 || { echo "build failed"; tail -5 "$logs/build.log"; exit 1; }
# pytest runs beside the synthetic groups and the gate, on the synthetic CPUs (it tests tools/, not the game: about 4 min
# less per verdict, user 2026-11-24 "y no hay nada mas que se pueda hacer"); its summary line is printed after the boot line.
taskset -c "$synth_cpus" .venv/bin/pytest --cov --cov-fail-under=80 -q > "$logs/pytest.log" 2>&1 &
pytest_pid=$!
ctest --test-dir build 2>&1 | grep -E 'tests passed|Failed'
# The synthetic groups run in the background while the full gate and the boot check run (user, 2026-11-21: "no hay forma de
# hacer más rápido el proceso en vez de 2 horas?"). They already had their own CPUs (GT_SYNTH_CPUS) and their own lock, apart
# from the gate's (GT_GATE_CPUS), so overlapping them changes no result; their lines are printed after the boot line, in the
# same form as before.
synth_out=$logs/synth.out
only_list=$PWD/$logs/synth_only.txt
# GT_VERDICT_SCOPE: a verdict_scope.py to run instead of the tree's (the runners pass the verdict branch's copy, so a PR
# based on an older main still gets the current scope rules).
if [ -n "${GT_VERDICT_SCOPE:-}" ]; then scope=$(.venv/bin/python "$GT_VERDICT_SCOPE" origin/main HEAD --only-file "$only_list" 2>/dev/null || echo synthetic)
else scope=$(.venv/bin/python -m tools.verdict_scope origin/main HEAD --only-file "$only_list" 2>/dev/null || echo synthetic); fi
(
# The synthetic gate only when the PR can reach it (tools/verdict_scope.py: decomp source, headers, harness, build, runtime
# patches; not docs, mods' checks, pad routes, renderer patches or the mod host; user, 2026-11-10).
# "only N": the PR changes nothing but decomp sources (src/game/**.cpp) and shape lines, so each group installs only the
# functions those files register (tools/verdict_scope.py; user 2026-11-24 "y no se pueden acelerar mas las verificaciones":
# each scene spent 650-950 s comparing all ~17.5k registered functions, verdict 842). Shared code runs every function.
only=""
case "$scope" in only*) only="--only @$only_list"; echo "synthetic: only the ${scope#only } functions the PR's decomp sources register";; esac
if [ "$scope" = skip ]; then
  echo "synthetic: skipped (no path the synthetic gate can see changed; tools/verdict_scope.py)"
else
  # Two groups, one after the other: a synthetic run is CPU-bound (unlike a gate route), and six at once ran so slowly that
  # every one hit its timeout (exit 124, verdict 526, 2026-11-10).
  # GT_VERIFY_SYNTH_GROUPS: the scene groups, space-separated. The third group holds the scenes PR 674's functions were verified
  # after (garage, challenge, dirt, manual, challenge2): every scene some function was verified after must stay in the verdict.
  for group in ${GT_VERIFY_SYNTH_GROUPS:-dealer,race save_garage,circuit,buy,options garage,challenge,dirt,manual,challenge2}; do
    t=v${tag}_${group%%,*}
    # Its own lock and its own cores (user, 2026-11-10): a synthetic run is CPU-bound, not paced, so it need not wait for
    # another verdict's real-time gate routes (GT_GATE_CPUS above); one synthetic group at a time on GT_SYNTH_CPUS.
    CPU_LOCK_DIR="${CPU_LOCK_DIR:-/tmp/claude-$(id -u)}/synth" GT_SYNTH_ROUTE=$group GT_GATE_CPU_LOCK="verdict synth $tag $group" \
      taskset -c "$synth_cpus" .venv/bin/python -m tools.decomp_gate synth "$t" $only 2>&1 \
      | grep -E "^all routes|exit [^02]" | head -3
    grep -ah "synthetic: [0-9]* functions in" recomp/analysis/gt_diff_${t}_*.log
    echo "mismatched: $(grep -ah 'mismatched [1-9]' recomp/analysis/gt_diff_${t}_*.log | grep -oE '@0x[0-9A-F]+' | sort -u | tr '\n' ' ')"
  done
fi
) > "$synth_out" 2>&1 &
synth_pid=$!
# GT_VERIFY_OVERLAP=0 (the default below 16 CPUs) waits for the synthetic groups first: on a 6-core/12-thread laptop the two
# CPU sets share physical cores, and the gate's real-time routes need theirs.
# Default 0: the gate and a synthetic group in the same tree cannot run at once (both verdicts 775 and 830 lost their
# fast gate entirely when overlapped), so the synthetic groups still finish first unless GT_VERIFY_OVERLAP=1.
# Since 2026-11-24 the default is 1 on 16 CPUs or more (the desktop's two sets are whole cores): the likely cause of 775/830
# was two builds in one build/ at once, and the gate's build now holds build/.gt-build.lock (tools/decomp_gate.py
# build_command); its build/recomp errors are no longer filtered out of the verdict either.
# (and only when the tree's decomp_gate has that lock: a branch older than #846 still waits)
# (and only for a PR-scoped synthetic gate: verdict 846, with every function installed, had three groups run past the
# 30-minute lock cap beside the gate; a PR-scoped group takes about 2 min)
overlap_default=0
case "$scope" in only*) [ "$(nproc)" -ge 16 ] && grep -q 'def build_command' tools/decomp_gate.py 2>/dev/null && overlap_default=1;; esac
[ "${GT_VERIFY_OVERLAP:-$overlap_default}" = 1 ] || wait $synth_pid
# The gate takes the CPU lock itself, around its routes only (GT_GATE_CPU_LOCK): its recomp and build never hold it.
# The gate installs only the PR's functions too when the scope says "only N" (user 2026-11-24, "Sí, con gate diario"): the
# race route took 344 s instead of 17 min and compared 175 of PR 732's functions instead of 111. Shared code runs the
# gate on every function, as before; main gets the full gate and the full synthetic gate once a day.
gate_only=""
# (only when the tree's decomp_gate has `full --only`: a branch older than #846 would refuse the argument and lose its gate)
case "$scope" in only*) grep -q 'full.add_argument("--only"' tools/decomp_gate.py 2>/dev/null \
  && { gate_only="--only @$only_list"; echo "gate: only the ${scope#only } functions the PR's decomp sources register"; };; esac
GT_GATE_CPU_LOCK="verdict full $tag" taskset -c "$gate_cpus" .venv/bin/python -m tools.decomp_gate full "v$tag" $gate_only 2>&1 \
  | grep -E "^all routes|exit [^0]|FLOOR|MISMATCH|no report|build failed|recomp failed|contract audit|save routes:|^error" | head -40  # 10 hid the "all routes" line behind problem lines (L575)
# Boot check: the save_garage pad route on the decompiled build from a fresh copy of the user's save.
boot=$logs/boot
# GT_VERIFY_BOOT_SECONDS (default 600): the boot route's wall-clock budget. On a loaded machine (load ~30 on 24 cores, powersave)
# save_garage needed more than 600 s and the boot check failed on time alone (verdicts 662, 666, 667 on 2026-11-19).
# It is a time budget only: the route's waits and the pass condition are unchanged.
rm -rf "$boot" && mkdir -p "$boot/sd" && cp -r game_files/UCES01245-GAMEDAT "$boot/sd/"
# GT_VERIFY_BOOT_EXIT_AFTER (vblanks, default 3600 = 60 s): the route ends that long after its script is done. The pass
# condition is the script reaching its end; without it the route idled in the garage to its whole budget (verdict 837:
# script done at 6 min of game, the step took 20 min). One minute of idle after the end is still run and checked.
GT_PAD_EXIT_AFTER=${GT_VERIFY_BOOT_EXIT_AFTER:-3600} GT_SAVEDATA_DIR=$boot/sd GT_FRAME_DUMP= tools/cpu_lock.sh "verdict boot $tag" tools/pad_route.sh save_garage "${GT_VERIFY_BOOT_SECONDS:-600}" "$boot/run" --decomp on
echo "boot save_garage rc=$(cat "$boot/run/run.exit") $(grep -c satisfied "$boot/run/run.log") waits $(grep -o 'script done.*' "$boot/run/run.log")"
rm -rf "$boot/sd"
wait $pytest_pid 2>/dev/null; tail -1 "$logs/pytest.log"
wait $synth_pid 2>/dev/null || true
cat "$synth_out"

