#!/usr/bin/env bash
# tools/delegate.sh [-C DIR] [-t TITLE] PROMPT : hand one decomp task to the delegate LLM (DeepSeek V4.1 Flash through
# OpenRouter, via opencode) and print its final answer. User decisions 2026-11-20 (CLAUDE.md § User decisions, 2026-11-20).
#
# The delegate works like Claude does: full access, in DIR (default: the current directory, normally the caller's own
# worktree), with permissions auto-approved (`opencode run --auto`). It reads AGENTS.md there like any agent; the hard
# limits below are prepended to every prompt as well. Its result is still a draft until the caller's gates pass.
# --standalone: a private server per session, so ending this script ends the session. Without it the session ran on
# in the shared opencode background service after its client was killed, and kept running gates and committing.
# Every session is logged (start and end) to $GT_DELEGATE_LOG so the orchestrating session can tell the user when one is open.
# Exit status: the answer on stdout with 0; 2 when the delegate is unavailable (no credit, auth, rate limit, network,
# empty answer): the caller stops delegating, asks the user, and does the work itself.
# Exit 3 when the session hit GT_DELEGATE_TIMEOUT (seconds, default 14400: a batch with gates waits for locks): not an
# outage, the work so far is in DIR. GT_DELEGATE_MODEL overrides the model.
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
- You only WRITE code: draft, build, static audits (tools.frame_audit, literal_check, sp_loads, arg_reads), local commit. Do NOT run tools.decomp_gate (no gate, synth, coverage, sabotage): Claude verifies after you.
- Kill only processes you started (no pkill -f). Never commit game data, recompiler output or the local CMakeLists GT_PREBUILT_GENERATED change. Do not access /mnt/veracrypt64.
- Decompiled bodies are hand-written structured C (named locals, G[n] only at the call boundary); a function is done only when its gate passes.

Task:
'
err=$(mktemp); trap 'rm -f "$err"' EXIT
echo "$(date '+%F %T') START pid=$$ dir=$dir title=$title model=$model" >> "$log"
out=$(cd "$dir" && timeout "${GT_DELEGATE_TIMEOUT:-14400}" opencode run --standalone --auto -m "$model" --title "$title" "$limits$1" 2>"$err")
rc=$?
echo "$(date '+%F %T') END   pid=$$ rc=$rc title=$title" >> "$log"
[ $rc -eq 124 ] && { echo "delegate session timed out; its work so far is in $dir" >&2; exit 3; }
if [ $rc -ne 0 ] || [ -z "${out//[[:space:]]/}" ]; then
  echo "delegate unavailable (exit $rc): $(tail -3 "$err" | tr '\n' ' ')" >&2
  exit 2
fi
printf '%s\n' "$out"
