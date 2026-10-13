# Differential harness — import record & replay (design)

Status: approved for implementation by the user on 2026-10-13 ("haz la primera", option 1 of the
post-batch-63 proposal), written without a separate review round at the user's request ("no pares").
Extends `docs/superpowers/specs/2026-10-10-phase3-diff-harness-design.md`.

## Goal

Make decompiled functions that call PSP imports comparable in `--decomp diff`. Today an isolated
run stops at the first import that isn't replay-safe and the case is "not comparable". The harness
logs at the end of batch 63 show 126 registered-or-tried functions never compared; 125 of them stopped
at an import (sceKernelWakeupThread 32, sceUmdWaitDriveStat 19, sceKernelCreateThread 19,
sceKernelSleepThread 17, sceKernelGetThreadId 6, sceKernelChangeCurrentThreadAttr 5, a tail of
others). Those functions — and the ~30 the batch agent left unregistered for this reason — are the
next decomp pool.

Success:
1. Full gate on the three routes: every function registered today stays equal, 0 mismatches.
2. Functions blocked only by recordable imports are compared (> 0 cases) and equal.
3. The new checks can fail: a sabotaged decompiled function that calls an import with a different
   argument, skips an import or calls an extra one is reported as mismatched.
4. Diff mode still never changes the game's run: every import executes exactly once, in the original's
   run, as it would in `--decomp off`.

## How it works

**Import gate.** In diff mode every import stub's registered function is replaced by a gate that
holds the original HLE handler. All guest dispatch goes through `rt.function_at(stub)` — the
harness's isolated runs, `call_guest`, tail calls — so the gate sees every import call. Outside a case
it calls the HLE unchanged (one indirection, diff mode only).

**Record (original's run).** In a case, an import on the *recordable* list runs for real and the gate
appends a record: stub address, the argument registers it reads (per-NID arity), the registers the HLE
changed (index → new value), the guest RAM it changed (4 KiB pages before/after, stored as changed
byte runs) and the kernel replay state before and after (`host/kernel_replay_state.hpp`). If the HLE
switches threads anyway the allowlist is wrong: the run stops with a message naming the NID.

**Replay (decompiled run).** The decompiled run must make the same import calls in the same order.
At each one the gate takes the next record and checks the stub and its argument registers (and that
the kernel replay state matches the record's "before"); a difference is a mismatch ("import call 2:
expected sceKernelCreateThread(a0=…), got …"). Then it applies the record — registers, RAM bytes,
kernel state — instead of running the HLE, and returns to ra. Ending with records left over, or calling
an import past the end, is a mismatch too.

**Replay-safe imports** (the existing list: LwMutex, interrupts, dcache, system time, …) keep running
in both runs as today; they are not recorded.

**Aborts continue live instead of re-running.** When the original's isolated run can't finish (an
import that is neither replay-safe nor recordable, the step/time bound, LwMutex contention), the
harness no longer restores RAM and re-runs the call — that would execute again any import already run
(a latent double execution today: "import inside …"). Instead the game continues *live* from where the
isolated run stopped: from the stop point itself, or — when the kernel refused an operation (isolation
broken) — from the registers saved just before the import that was refused, so it runs for real. The
original's return to the sentinel address is caught by a handler registered there, which jumps to the
real return address saved at the start of the case. New cases don't start while a continuation is
pending (single slot, cleared when the sentinel is reached).

**Recordable list** (NIDs, in `host/main.cpp` beside the replay-safe list, each with a one-line
reason, starting with the ones the logs show and whose HLE never switches threads):
sceUmdWaitDriveStat (returns at once), sceKernelCreateThread (creates a thread object; no switch until
StartThread), sceKernelGetThreadId, sceKernelChangeCurrentThreadAttr, scePowerGetCpuClockFrequencyInt,
sceKernelPowerTick, sceSasGetOutputmode, sceFontFindOptimumFont, sceNpDrmSetLicenseeKey. Further NIDs
are added the same way, after reading their HLE. Never: imports that block or always switch
(sceKernelSleepThread, sceDisplayWaitVblankStart*, sceKernelDelayThread, waits, sceUtilityLoadModule
which delays the caller).

**sceKernelWakeupThread** wakes a thread and preempts when the woken one has a higher priority. In
a case it is recordable only when it would not preempt: under kernel isolation GT's handler checks
first and, if the wake would switch threads, refuses (does nothing) and breaks isolation — the case
then continues live from just before the call, exactly like LwMutex contention today. If the thread
table isn't reachable from `host/gt_kernel.cpp` without a new lcs-recomp patch, this part is left for a
follow-up and noted in FINDINGS.

**Report.** "first not comparable: original: import stub 0x… (sceXxx)" names the import; a summary at
the end counts not-comparable cases per import, so the next NID to make recordable is evident.

## Decisions

- **Replay, not re-execution.** Running an HLE twice changes host state the RAM snapshot doesn't
  cover (files, threads, audio, uids). Recording its effect once and applying it in the second run
  keeps the game's run identical to `--decomp off`.
- **Arguments compared by arity, not all eight registers.** Decompiled C++ doesn't reproduce stale
  values in registers an import never reads; comparing them would report false mismatches.
- **RAM effect by page diff.** The HLE writes guest RAM in many ways (`store*`, `copy_in`, raw
  pointers). Snapshotting the 4 KiB pages before/after each recorded call catches all of them without
  touching every HLE; the cost (a 32 MiB compare per recorded call) only applies to sampled cases.
- **Known limit:** an import that *reads* guest memory (e.g. a name string) is checked only through
  its argument registers; if the decompiled run wrote different bytes there before the call, the
  final RAM comparison catches it only if those bytes survive to the end. Pointed-to input data can be
  added to a record later for specific NIDs.
- **Continuation over restore.** It removes the double-execution hazard and saves a full re-run of
  the call; the sampling slot is the only new shared state.

## Testing

- `host/import_replay.{hpp,cpp}`: the record/replay log is a pure component (no Runtime): append a
  record from before/after register sets and RAM spans; match a call against the next record (stub,
  arity-limited arguments); apply a record to registers and a RAM span; report leftovers. Unit tests
  in `host/tests/import_replay_test.cpp` (CTest), written first.
- Gate integration, measured on the three routes: the full gate before/after (no regressions, fewer
  not-comparable cases, per-import counts); `GT_DECOMP_ONLY` runs over previously blocked functions
  re-registered from the batch 56–63 "can't replay" lists.
- Proof the checks fail: a temporary sabotage that changes one import argument, and one that drops an
  import call, in a function that became comparable — both reported as mismatched.
- `--decomp off` and `on` unchanged: the gate is installed only in diff mode.

## Out of scope

- Imports that always block or switch threads (Sleep, waits, vblank): a case can't run them in
  isolation; they need a scheduler-aware harness.
- Deferring a preemption to the end of the case (would change thread interleaving in diff mode).
- Recording pointed-to input buffers of imports.
