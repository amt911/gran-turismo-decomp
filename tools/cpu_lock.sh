#!/bin/sh
# usage: tools/cpu_lock.sh WHAT COMMAND... -> runs COMMAND holding /tmp/claude-<uid>/cpu-heavy.lock, shared with other agent
# sessions, for timing-dependent jobs only. A hold ends by itself after 30 min (timeout). A waiter goes ahead at once when
# the holder is orphaned (its process or its agent session is gone), and after 30 min of waiting in any case.
D=/tmp/claude-$(id -u); LOCK=$D/cpu-heavy.lock; OWNER=$D/cpu-heavy.owner
what=$1; shift
session_pid() {  # the agent session (the nearest `claude` ancestor), or the parent shell if there is none
  p=$$; while [ "$p" -gt 1 ]; do [ "$(cat /proc/$p/comm 2>/dev/null)" = claude ] && { echo $p; return; }; p=$(awk '{print $4}' /proc/$p/stat 2>/dev/null || echo 1); done; echo $PPID
}
alive() { case "$1" in ""|*[!0-9]*) return 0;; esac; kill -0 "$1" 2>/dev/null; }  # a missing or non-numeric pid counts as alive
exec 9>"$LOCK"
waited=0
until flock -n 9; do
  read -r hpid _ spid _ < "$OWNER" 2>/dev/null
  if { [ -n "$hpid" ] && { ! alive "$hpid" || ! alive "$spid"; }; } || [ $waited -ge 1800 ]; then
    echo "[cpu-lock] holder orphaned or 30 min passed; stopping: $(cat "$OWNER" 2>/dev/null)" >&2
    alive "$hpid" && { kill -TERM "$hpid"; sleep 15; kill -KILL "$hpid" 2>/dev/null; }
    flock -w 120 9 && break
    echo "[cpu-lock] still held; giving up" >&2; exit 75
  fi
  sleep 30; waited=$((waited + 30))
done
echo "$$ gran-turismo-decomp $(session_pid) $(date +%FT%T) $what" > "$OWNER"
timeout 1800 "$@"; rc=$?
: > "$OWNER"
exit $rc
