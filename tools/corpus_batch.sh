#!/bin/bash
# One batch of the synthetic-corpus search end to end (docs/superpowers/specs/2026-11-11-coverage-guided-synthetic-inputs-design.md):
# coverage search over the batch's functions, a corpus-off and a corpus-on synthetic run, the numbers, a commit + push of the new
# recipes, the sabotage pair (corpus on must pin a newly verified function, corpus off must not), then the verdict on a second tree.
#   tools/corpus_batch.sh N LIST [BRANCH]      LIST: the batch's addresses (tools.corpus_batch plan writes batchN.txt)
# Needs a branch off main checked out when it starts; run from the repo root, never while another gate is running on this machine's
# build dir. Everything heavy goes through tools.decomp_gate (gt-heavy.slice, CPU lock). Logs: recomp/analysis/corpus_batch_N/.
# The PR (title, numbers, verdict comment) is written by hand from corpus_batch_N/summary.txt and the verdict log.
set -u
N=$1 LIST=$2
cd "$(git rev-parse --show-toplevel)" || exit 1
logs=recomp/analysis/corpus_batch_$N
mkdir -p "$logs"
py=.venv/bin/python
rm -rf build/coverage
GT_SYNTH_SEARCH_IDLE=60 GT_GATE_CPU_LOCK="synth search batch$N" $py -m tools.decomp_gate coverage "s${N}a" --only "@$LIST" --synth \
  --search 200 --search-seconds 10 > "$logs/search.log" 2>&1
cmake -S . -B build -DGT_COVERAGE_FILES= > /dev/null 2>&1  # back to the normal build
GT_SYNTH_CORPUS=off GT_GATE_CPU_LOCK="synth before $N" $py -m tools.decomp_gate synth "s${N}b" --only "@$LIST" > "$logs/before.log" 2>&1
GT_GATE_CPU_LOCK="synth after $N" $py -m tools.decomp_gate synth "s${N}c" --only "@$LIST" > "$logs/after.log" 2>&1
b=recomp/analysis/gt_diff_s${N}b_dealer.log a=recomp/analysis/gt_diff_s${N}c_dealer.log
{ $py -m tools.corpus_batch summary "$b" "$a"; $py -m tools.corpus_batch pick "$b" "$a"; } > "$logs/summary.txt"
cat "$logs/summary.txt"
read -r addr file < <($py -m tools.corpus_batch pick "$b" "$a")
git add config/synth_corpus
git commit -q -m "feat(config): synthetic corpus batch $N ($(head -1 "$logs/summary.txt"))

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01CPECciFYErXe7y49BY4UVc" && git push -q -u origin "$(git branch --show-current)"
if [ -n "${addr:-}" ] && [ "$addr" != none ]; then
  GT_GATE_CPU_LOCK="sabotage corpus on" $py -m tools.decomp_gate sabotage "s${N}d" --file "$file" --only "$addr" --synth > "$logs/sabotage_on.log" 2>&1
  GT_SYNTH_CORPUS=off GT_GATE_CPU_LOCK="sabotage corpus off" $py -m tools.decomp_gate sabotage "s${N}e" --file "$file" --only "$addr" --synth \
    > "$logs/sabotage_off.log" 2>&1
fi
GT_VERIFY_TREE=${GT_VERIFY_TREE:-$(dirname "$PWD")/gt-verify3} tools/verdict.sh "$(git branch --show-current)" "c$N" > "$logs/verdict.log" 2>&1
echo "corpus_batch $N done: $logs"
