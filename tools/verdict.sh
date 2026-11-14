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
heavy sh -c 'cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ > /dev/null && cmake --build build -j8' \
  > "$logs/build.log" 2>&1 || { echo "build failed"; tail -5 "$logs/build.log"; exit 1; }
.venv/bin/pytest --cov --cov-fail-under=80 -q > "$logs/pytest.log" 2>&1; tail -1 "$logs/pytest.log"
ctest --test-dir build 2>&1 | grep -E 'tests passed|Failed'
# The gate takes the CPU lock itself, around its routes only (GT_GATE_CPU_LOCK): its recomp and build never hold it.
GT_GATE_CPU_LOCK="verdict full $tag" taskset -c "$gate_cpus" .venv/bin/python -m tools.decomp_gate full "v$tag" 2>&1 \
  | grep -E "^all routes|exit [^0]|FLOOR|MISMATCH|no report" | head -40  # 10 hid the "all routes" line behind problem lines (L575)
# Boot check: the save_garage pad route on the decompiled build from a fresh copy of the user's save.
boot=$logs/boot
rm -rf "$boot" && mkdir -p "$boot/sd" && cp -r game_files/UCES01245-GAMEDAT "$boot/sd/"
GT_SAVEDATA_DIR=$boot/sd GT_FRAME_DUMP= tools/cpu_lock.sh "verdict boot $tag" tools/pad_route.sh save_garage 600 "$boot/run" --decomp on
echo "boot save_garage rc=$(cat "$boot/run/run.exit") $(grep -c satisfied "$boot/run/run.log") waits $(grep -o 'script done.*' "$boot/run/run.log")"
rm -rf "$boot/sd"
# The synthetic gate only when the PR can reach it (tools/verdict_scope.py: decomp source, headers, harness, build, runtime
# patches; not docs, mods' checks, pad routes, renderer patches or the mod host; user, 2026-11-10).
scope=$(.venv/bin/python -m tools.verdict_scope origin/main HEAD 2>/dev/null || echo synthetic)
if [ "$scope" = skip ]; then
  echo "synthetic: skipped (no path the synthetic gate can see changed; tools/verdict_scope.py)"
else
  # Two groups, one after the other: a synthetic run is CPU-bound (unlike a gate route), and six at once ran so slowly that
  # every one hit its timeout (exit 124, verdict 526, 2026-11-10).
  for group in dealer,race save_garage,circuit,buy,options; do
    t=v${tag}_${group%%,*}
    # Its own lock and its own cores (user, 2026-11-10): a synthetic run is CPU-bound, not paced, so it need not wait for
    # another verdict's real-time gate routes (GT_GATE_CPUS above); one synthetic group at a time on GT_SYNTH_CPUS.
    CPU_LOCK_DIR="${CPU_LOCK_DIR:-/tmp/claude-$(id -u)}/synth" GT_SYNTH_ROUTE=$group GT_GATE_CPU_LOCK="verdict synth $tag $group" \
      taskset -c "$synth_cpus" .venv/bin/python -m tools.decomp_gate synth "$t" 2>&1 \
      | grep -E "^all routes|exit [^02]" | head -3
    grep -ah "synthetic: [0-9]* functions in" recomp/analysis/gt_diff_${t}_*.log
    echo "mismatched: $(grep -ah 'mismatched [1-9]' recomp/analysis/gt_diff_${t}_*.log | grep -oE '@0x[0-9A-F]+' | sort -u | tr '\n' ' ')"
  done
fi
