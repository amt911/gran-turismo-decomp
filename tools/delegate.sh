#!/usr/bin/env bash
# tools/delegate.sh [-C DIR] [-t TITLE] PROMPT : hand one decomp task to the delegate LLM (DeepSeek V4.1 Flash through
# OpenRouter, via opencode) and print its final answer. User decisions 2026-11-20 (CLAUDE.md § User decisions, 2026-11-20).
#
# The delegate works like Claude does: full access, in DIR (default: the current directory, normally the caller's own
# worktree), with permissions auto-approved (`opencode run --auto`). It reads AGENTS.md there like any agent; the hard
# limits below are prepended to every prompt as well. Its result is still a draft until the caller's gates pass.
# Every session is logged (start and end) to $GT_DELEGATE_LOG so the orchestrating session can tell the user when one is open.
# Exit status: the answer on stdout with 0; 2 when the delegate is unavailable (no credit, auth, rate limit, network,
# timeout, empty answer): the caller stops delegating, asks the user, and does the work itself.
# GT_DELEGATE_MODEL overrides the model; GT_DELEGATE_TIMEOUT (seconds, default 1800) bounds one session.
set -uo pipefail
dir=$PWD title="gt-delegate"
while getopts "C:t:" o; do case $o in C) dir=$OPTARG ;; t) title=$OPTARG ;; *) exit 64 ;; esac; done
shift $((OPTIND - 1))
[ $# -eq 1 ] && [ -n "$1" ] || { echo "usage: tools/delegate.sh [-C DIR] [-t TITLE] PROMPT" >&2; exit 64; }
model=${GT_DELEGATE_MODEL:-openrouter/deepseek/deepseek-v4.1-flash-1017}
log=${GT_DELEGATE_LOG:-/home/andres/repos/gt-ops/out/opencode-sessions.log}
mkdir -p "$(dirname "$log")"
limits='Hard limits (they bind you exactly like they bind Claude in this repo; AGENTS.md has the rest):
- Never merge anything, never push main, never force-push. Push only feature branches you were told to push.
- Heavy jobs (recomp, builds, gates, pytest with workers) run inside the gt-heavy.slice memory cgroup (AGENTS.md); builds with clang at -j4.
- Every tools.decomp_gate run sets GT_GATE_CPU_LOCK="<desc>" and is preceded by: until ! /home/andres/repos/gt-ops/verdict_waiting.sh; do sleep 60; done
- Kill only processes you started (no pkill -f). Never commit game data, recompiler output or the local CMakeLists GT_PREBUILT_GENERATED change. Do not access /mnt/veracrypt64.
- Decompiled bodies are hand-written structured C (named locals, G[n] only at the call boundary); a function is done only when its gate passes.

Task:
'
err=$(mktemp); trap 'rm -f "$err"' EXIT
echo "$(date '+%F %T') START pid=$$ dir=$dir title=$title model=$model" >> "$log"
out=$(cd "$dir" && timeout "${GT_DELEGATE_TIMEOUT:-1800}" opencode run --auto -m "$model" --title "$title" "$limits$1" 2>"$err")
rc=$?
echo "$(date '+%F %T') END   pid=$$ rc=$rc title=$title" >> "$log"
if [ $rc -ne 0 ] || [ -z "${out//[[:space:]]/}" ]; then
  echo "delegate unavailable (exit $rc): $(tail -3 "$err" | tr '\n' ' ')" >&2
  exit 2
fi
printf '%s\n' "$out"
