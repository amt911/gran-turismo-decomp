#!/usr/bin/env bash
# tools/delegate.sh PROMPT : send one decomp drafting task to the delegate LLM (DeepSeek V4.1 Flash through OpenRouter,
# via opencode) and print its answer. User decision 2026-11-20 (CLAUDE.md § User decisions, 2026-11-20).
#
# The delegate runs in a fresh empty directory, so it cannot read or change any checkout: everything it needs (the asm from
# tools.disasm, the rules, a style example) goes in the prompt, and its answer is a draft that Claude reviews and gates.
# Exit status: 0 with the answer on stdout; 2 when the delegate is unavailable (no credit, auth, rate limit, network,
# timeout, empty answer) -- the caller then stops delegating, asks the user, and does the work itself.
# GT_DELEGATE_MODEL overrides the model; GT_DELEGATE_TIMEOUT (seconds, default 600) bounds one call.
set -uo pipefail
[ $# -eq 1 ] && [ -n "$1" ] || { echo "usage: tools/delegate.sh PROMPT" >&2; exit 64; }
model=${GT_DELEGATE_MODEL:-openrouter/deepseek/deepseek-v4.1-flash-1017}
dir=$(mktemp -d "${TMPDIR:-/tmp}/gt-delegate.XXXXXX")
trap 'rm -rf "$dir"' EXIT
out=$(cd "$dir" && timeout "${GT_DELEGATE_TIMEOUT:-600}" opencode run -m "$model" "$1" 2>"$dir/.err")
rc=$?
if [ $rc -ne 0 ] || [ -z "${out//[[:space:]]/}" ]; then
  echo "delegate unavailable (exit $rc): $(tail -3 "$dir/.err" | tr '\n' ' ')" >&2
  exit 2
fi
printf '%s\n' "$out"
