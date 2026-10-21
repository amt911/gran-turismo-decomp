# Agent orchestration

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Agent orchestration — parallel where it's free, batched where it's yours

Delegating work to agents moves the bottleneck from typing to **scheduling**: what waits on what,
what each agent has to rediscover, and which decisions quietly stop being yours. Same convention as
[Debugging](debugging.md#debugging--keep-the-loop-from-running-away) — every rule carries the number it came
from, out of the same measured 15 h session.

- **Review is not on the critical path.** Reviewing task N and starting task N+1 are independent
  whenever they touch different files. Serialized, review is **10-15% of the wall clock** and blocks
  everything queued behind it; run in parallel it costs nothing at all. **On receiving an
  implementation report, dispatch its review and the next implementation in the same turn.**
  This is the one sanctioned exception to *"at most 1 agent at a time"* in
  [normal mode](../../AGENTS.md#mode-switch): the cap is one **implementation** agent. A review agent reads and
  reports — it writes nothing, so it cannot race the implementer.
  **Here:** one implementation agent in normal mode, plus its reviewer. The **exclusive resources** —
  at most one agent touching each — are: the **build directory** of a worktree (two builds in one
  tree corrupt each other's objects; one agent = one git worktree), the **REA MCP session** (its
  "active target" is global to the server, so a second `open_binary` silently switches the first
  agent's target), the **Ghidra project lock** (`ghidra/`), and each **PPSSPP instance / debugger
  port**, and each running **game instance** of the recompiled build. Decompiling *different units in different worktrees* parallelizes freely once the user
  lifts the cap ("modo desatendido" or explicit instruction).
- **Keep one shared facts file.** Every fresh agent rediscovers the same things: the real selector,
  which fake already exists, what that helper actually accepts. Keep `docs/FACTS.md` in the
  workspace, have each agent append to it when it finishes, and hand it to the next one in its
  dispatch. What belongs there: **facts verified against the repo or the device**, never opinions or
  plans. It is not `docs/FINDINGS.md` and does not replace it — FINDINGS holds the durable gotcha
  that is *not* deducible from the code, FACTS holds what is perfectly deducible and merely
  expensive to rediscover, and FACTS is allowed to go stale and die with the branch.
- **Plans carry contracts, not literal code.** The agent **trusts** the code you put in the plan; if
  you never compiled it, you have written an error wearing authority. **Measured: 4 wrong code
  blocks, 15-40 min of detour each.** Write exact names, exact signatures, and "mirror the shape of
  `<X>`" — claims the agent can verify against the repo — and reserve literal code for what you have
  actually run. This is what `writing-plans` produces; keep it that way when you edit the plan by
  hand.
- **Batch the discretionary decisions.** The work that shows up along the way — a capability being
  quietly dropped, a missing script, an adjacent bug — added up to **5-6 h of 15**. Every one was
  justified on its own; deciding them as they appear is what takes them away from you. Accumulate
  them and ask **once per batch, with the estimated cost of each**. In **"modo desatendido"** there
  is nobody to ask, so the batch goes into the PR body as a list with its costs — the decision is
  still yours, it just moves to review time.
- **What never gets cut.** With the numbers on the table: review was **1.5 h of 15**, and it found a
  `create()` silently discarding fields, a 404 caused by SQL deduplication, a silent merge that
  corrupted data, a delete-and-recreate with no transaction, and several inert assertions. **Cutting
  review does not give time back; it defers it to production.** If something has to be cut, cut
  reproduction (write the shortcut — see
  [Budget the lap](debugging.md#the-loop-is-the-cost)) and cut serialization (dispatch the review in parallel).
  Never verification.

### Day one — the numbers that fill the blanks

Three measurements, taken once at the start of a project, turn the rules above into something
enforceable. None of them takes more than an afternoon.

**Status (2026-10-09):** 2 is done (the exclusive resources are named above). 1 and 3 wait for the
Phase 2–3 build and harness — there is no lap to time and no suite to break yet.

1. **The lap** — time `build → deploy → reach the state → observe` once, on a real bug if there is
   one, and write the seconds into the table in
   [The loop is the cost](debugging.md#the-loop-is-the-cost). Whichever phase dominates is the one that gets a
   shortcut script; the rest are noise and stay unoptimized.
2. **The exclusive resource** — name the thing only one agent can hold at a time (emulator, dev
   database, dev-server port, a physical device) and write it into the orchestration note above.
   Everything else parallelizes; this is the one that corrupts a run when two agents touch it.
3. **The inert assertions** — break one assertion on purpose and run the suite. Anything still green
   is inert. Then walk the table in [Tests that cannot fail](debugging.md#tests-that-cannot-fail), delete the
   rows this stack cannot produce, and name the mechanism for the ones it can.

Record all three in this file, not in a session — the point is that the next session inherits them.

## Sharing the CPU with other agent sessions — real-time jobs only

Another agent session (another project) may share this machine for a whole night. The full differential gate, its
sabotage runs and the pacing measurements run real-time game routes that **fail under foreign load** (`WAIT seek …
timed out`): three gates failed on 2026-10-21 at load 27–32 from another session's `cargo` jobs, and each route passed
alone. Builds, `pytest` and `ctest` only get slower under load; they share the CPU normally and never take the lock.

- **`tools/cpu_lock.sh WHAT COMMAND...`** holds `flock /tmp/claude-<uid>/cpu-heavy.lock` around COMMAND and writes
  `<holder pid> <session name> <session pid> <start> <what>` to `cpu-heavy.owner`. The agreement with the other session
  goes through `SendMessage` (find it with `ListAgents`).
- **Holds stay under 30 min** and run under `timeout 1800`; split longer work (one sabotage file per hold).
- **Nobody waits forever:** a waiter goes ahead at once when the holder or its agent session is gone (a stopped session
  leaves its job behind), and after 30 min in any case (the user's rule; a peer may run niced instead of killing).
- **Never rewrite the script while a job uses it** — a running shell reads it as it goes; a chain started under the old
  file died with a syntax error. It lives in the repo for that reason.
- Before debugging a real-time failure, read `uptime`/`top`: foreign load is a rerun under the lock, not a bug.
