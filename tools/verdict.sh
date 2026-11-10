#!/usr/bin/env bash
# The PR verdict pass (docs/agents/pr-verification.md) on a clean checkout of a pushed branch: setup, extract, recomp,
# a clang build, pytest with coverage, ctest, the full differential gate, the boot check and the synthetic gate after
# every route some function was verified after. Prints one line per step; the logs stay under recomp/analysis/.
#   tools/verdict.sh <branch> <tag>        (GT_VERIFY_TREE: the worktree to use, default ../gt-verify)
# Kept in the repo: a scratch copy was wiped by a reboot once (FINDINGS / lessons).
set -u
branch=$1 tag=$2
tree=${GT_VERIFY_TREE:-$(dirname "$(git rev-parse --show-toplevel)")/gt-verify}
cd "$tree" || exit 1
git fetch -q origin && git checkout -q --detach "origin/$branch" || { echo "no branch $branch"; exit 1; }
echo "== head $(git rev-parse --short HEAD)"
logs=recomp/analysis/verdict_$tag
mkdir -p "$logs"
heavy() { systemd-run --user --scope --quiet --slice=gt-heavy.slice -- "$@"; }
tools/setup.sh > "$logs/setup.log" 2>&1 || { echo "setup failed"; exit 1; }
.venv/bin/python -m tools.extract --full > "$logs/extract.log" 2>&1 || { echo "extract failed"; exit 1; }
heavy .venv/bin/python -m tools.recomp > "$logs/recomp.log" 2>&1 || { echo "recomp failed"; tail -3 "$logs/recomp.log"; exit 1; }
heavy sh -c 'cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ > /dev/null && cmake --build build -j8' \
  > "$logs/build.log" 2>&1 || { echo "build failed"; tail -5 "$logs/build.log"; exit 1; }
.venv/bin/pytest --cov --cov-fail-under=80 -q > "$logs/pytest.log" 2>&1; tail -1 "$logs/pytest.log"
ctest --test-dir build 2>&1 | grep -E 'tests passed|Failed'
# The gate takes the CPU lock itself, around its routes only (GT_GATE_CPU_LOCK): its recomp and build never hold it.
GT_GATE_CPU_LOCK="verdict full $tag" taskset -c 6-11,18-23 .venv/bin/python -m tools.decomp_gate full "v$tag" 2>&1 \
  | grep -E "^all routes|exit [^0]|FLOOR|MISMATCH|no report" | head
# Boot check: the save_garage pad route on the decompiled build from a fresh copy of the user's save.
boot=$logs/boot
rm -rf "$boot" && mkdir -p "$boot/sd" && cp -r game_files/UCES01245-GAMEDAT "$boot/sd/"
GT_SAVEDATA_DIR=$boot/sd GT_FRAME_DUMP= tools/cpu_lock.sh "verdict boot $tag" tools/pad_route.sh save_garage 600 "$boot/run" --decomp on
echo "boot save_garage rc=$(cat "$boot/run/run.exit") $(grep -c satisfied "$boot/run/run.log") waits $(grep -o 'script done.*' "$boot/run/run.log")"
rm -rf "$boot/sd"
# The synthetic gate only when the PR can reach it (tools/verdict_scope.py: decomp source, headers, harness, build, runtime
# patches; not docs, mods' checks, pad routes, renderer patches or the mod host), all six routes in one run: the gate keeps
# MAX_PARALLEL_ROUTES alive, so one run takes the longest route's time instead of two groups one after the other (user, 2026-11-10).
scope=$(.venv/bin/python -m tools.verdict_scope origin/main HEAD 2>/dev/null || echo synthetic)
if [ "$scope" = skip ]; then
  echo "synthetic: skipped (no path the synthetic gate can see changed; tools/verdict_scope.py)"
else
  group=dealer,race,save_garage,circuit,buy,options
  t=v${tag}_synth
  GT_SYNTH_ROUTE=$group GT_GATE_CPU_LOCK="verdict synth $tag" .venv/bin/python -m tools.decomp_gate synth "$t" 2>&1 \
    | grep -E "^all routes|exit [^02]" | head -3
  grep -ah "synthetic: [0-9]* functions in" recomp/analysis/gt_diff_${t}_*.log
  echo "mismatched: $(grep -ah 'mismatched [1-9]' recomp/analysis/gt_diff_${t}_*.log | grep -oE '@0x[0-9A-F]+' | sort -u | tr '\n' ' ')"
fi
