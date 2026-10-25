# Synthetic cases — verifying functions no route reaches

Status: approved by the user on 2026-10-25 ("quiero que hagas los casos sintéticos… apruebo todo"), after the status
report showed the 17 routes reach almost no undecompiled game code (7368 / 23208 functions decompiled, 31.7 %).

## Problem

The differential gate compares a decompiled function with its recompiled original only when a pad route calls it. The
17 routes execute about 8,000 functions; the other ~15,800 never run, so they cannot be verified. A new menu route
adds 2-7 functions, so more routes alone will not reach 100 %. 1,657 functions that are already decompiled are also
never compared by the full gate.

## Idea

Same gate, a different source of cases. At the end of a route, with the game's real memory as the backdrop, the host
calls each target function itself with generated inputs. Every call is a normal harness case: the original runs
isolated, then the decompiled version runs from the same state, and the results are compared (return registers, every
RAM page either run wrote, kernel replay state, import record/replay). The comparison is the existing one; only the
inputs are new.

## Inputs

A case is deterministic in (function address, case index, seed):

- **Arena**: 64 KiB of guest RAM, rewritten before each case. Its words are pointers into the arena (aligned), small
  integers, zero, -1, common floats and, in a few "table" blocks, addresses of trivial real game functions (functions
  that only return), so a virtual call through an arena object reaches real code instead of ending the case.
- **Registers**: a0-a3 and t0-t3 each take an arena pointer, a small integer, zero, -1 or a random word; f12-f19 take
  common floats. sp is the thread's sp less a margin; ra is the case's sentinel, as in a route case.
- The arena sits 1 MiB below the thread's stack. The run ends after the synthetic phase, so the game never sees what
  the arena overwrote, and every case restores the pages it wrote.

## Outcomes per case

- The original faults (a guest access outside RAM, a jump to no function, a non-replayable import, the step or time
  bound): **not comparable**. The case says nothing about the decompiled code.
- The original returns and the decompiled run faults, or any compared state differs: **mismatch**.
- Otherwise: **equal**.

A function is **verified by synthetic cases** when it has at least one equal case, no mismatches, and the sabotage
(`tools.decomp_gate sabotage --synth`) is detected on it. That is weaker evidence than a route case, because the inputs
are not the game's, and it is reported apart: the gate's route numbers and floor do not change.

## Where it runs

`gt_native --decomp diff` with `GT_SYNTH=<cases per function>` (and `GT_SYNTH_SEED`): route cases are off, so the
report counts only synthetic cases. When the pad script has been done for `GT_PAD_EXIT_AFTER` vblanks, the next pad
read (`sceCtrlReadBufferPositive`, on the game's main thread) runs every installed function's cases on a copy of that
thread's context, then stops the runtime ("Synthetic cases done"). One route, chosen for its state (the default is
`race`), is enough.

`tools.decomp_gate synth TAG --only ...` builds, runs that route with the variables set and prints the per-function
report. `tools.decomp_gate sabotage TAG --synth ...` runs the sabotage over synthetic cases.

## Not in scope (later, if the first results call for it)

- Inputs taken from real objects (live pointers of the right class).
- Changing what "done" means in AGENTS.md beyond adding the synthetic category. A function verified only by synthetic
  cases is listed as such in the README and ROADMAP counts.

## Testing

- C++ unit tests for the generator (`host/tests/synth_case_test.cpp`): deterministic for the same inputs, every pointer
  register and arena pointer word inside the arena and aligned, table blocks holding only the given functions, a
  different case index giving different inputs.
- The harness path is proven end to end: a known-good function compares equal, the same function sabotaged mismatches
  (and `sabotage --synth` pins it).
- `tools.decomp_gate` changes are TDD in pytest (command lines, report parsing), coverage ≥ 80 %.
