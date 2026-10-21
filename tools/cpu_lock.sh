#!/bin/sh
# usage: tools/cpu_lock.sh WHAT COMMAND... -> runs COMMAND holding /tmp/claude-<uid>/cpu-heavy.lock, shared with other agent
# sessions, for timing-dependent jobs only (docs/agents/agent-orchestration.md). A hold ends by itself after 30 min
# (timeout). A waiter goes ahead at once when the holder's agent session is gone, and after 30 min of waiting in any case:
# it stops whatever still holds the lock (inherited descriptors included) and keeps waiting until the lock is free.
D=/tmp/claude-$(id -u); LOCK=$D/cpu-heavy.lock; OWNER=$D/cpu-heavy.owner
what=$1; shift
session_pid() {  # the agent session (the nearest `claude` ancestor), or the parent shell if there is none
  p=$$; while [ "$p" -gt 1 ]; do [ "$(cat /proc/$p/comm 2>/dev/null)" = claude ] && { echo $p; return; }
    p=$(awk '{print $4}' /proc/$p/stat 2>/dev/null || echo 1); done; echo $PPID
}
holders() {  # processes with the lock file open that are not waiting for it (waiters: flock, this script)
  for p in $(fuser "$LOCK" 2>/dev/null); do
    [ "$p" = $$ ] && continue
    case "$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null)" in flock\ *|*cpu_lock*|*cpu-lock*) continue;; esac
    echo "$p"
  done
}
exec 9>"$LOCK"
waited=0; stopped=""
until flock -n 9; do
  owner=$(cat "$OWNER" 2>/dev/null); spid=$(echo "$owner" | cut -d' ' -f3)
  gone=false
  case "$spid" in ""|*[!0-9]*) ;; *) kill -0 "$spid" 2>/dev/null || gone=true;; esac  # v1 lines: no session field
  if { $gone || [ $waited -ge 1800 ]; } && [ "$stopped" != "$owner" ]; then
    echo "[cpu-lock] holder's session gone or 30 min passed; stopping the holders of: $owner" >&2
    pids=$(holders); [ -n "$pids" ] && { kill -TERM $pids 2>/dev/null; sleep 15; kill -KILL $pids 2>/dev/null; }
    stopped=$owner
  fi
  sleep 30; waited=$((waited + 30))
done
echo "$$ gran-turismo-decomp $(session_pid) $(date +%FT%T) $what" > "$OWNER"
timeout 1800 "$@"; rc=$?
: > "$OWNER"
exit $rc
