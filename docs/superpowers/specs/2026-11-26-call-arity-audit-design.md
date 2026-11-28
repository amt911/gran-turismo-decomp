# Call arity audit — design

Approved by the user on 2026-11-26 ("venga dale"). Status: implemented in `tools/call_audit.py`.

## Problem

Two bugs reached `main` and broke play under `--decomp on` (user report 2026-11-26). The gates could not see either of them:

- **`func_088D8918`.** It stored a constant instead of a callee's v1. It also called a sleep without its argument, `a0 = 10000`. The game hung on the GT logo after leaving a Starting Grid.
- **`func_08A17818`.** It called `func_08B855E0(0x08CF6244)` without `a1 = node + 8`, so the brand logos vanished.

Both are the same class of error: **the C call passes fewer argument registers than the original sets for a callee that reads them, or it reads `v1` after a helper that returns only `v0`.**

`call_guest` runs the callee on a copy of the caller's context, so a missing argument is not zero: it is whatever the decompiled caller's context holds, usually its own entry value. The callee then computes something plausible. A synthetic case compares the decompiled body with the original on the same inputs, but the stale register is the same in both runs only when the original also passed it through. A function that sleeps or blocks is `not comparable` and never compared at all.

## What the audit checks

`.venv/bin/python -m tools.call_audit [FILE.cpp ...]` reads every decompiled body (default: `src/**/*.cpp`) and the original asm from `extracted/EBOOT.elf`. It reports each call site that matches one of two rules.

1. **Missing argument.** Take a body's C call to a constant target `T`, which passes `N` registers in its argument list.
   - **Original side.** At a `jal T` in the original, the window is the instructions since the previous `jal`, or since the function's start, up to the delay slot. Within it, find the argument registers (`a0`–`a3`, `t0`–`t3`) that the original writes and that `T` reads.
   - **Callee side.** `T` reads a register when it uses it before writing it. A tail call and a `jal` inside `T` count as reads of the registers they need and `T` has not written. This is computed transitively and memoised.
   - **Rule.** A register at position `>= N` in that set is flagged, unless the C body assigns that register itself (`G[k] = ...` or `ctx.gpr[k] = ...`), as the emitter-style bodies do.
   - **Matching.** Several calls to the same target are matched by target: the smallest C argument count is compared with the largest need.
2. **Lost v1.** The C reads `ctx.gpr[3]` or `G[3]` after a call made through a helper that returns only `v0`, with no assignment to it in between. Helpers that copy `result.v1` (`call_keep`, `call_g`, `callf`, `call_virtual`, …) are recognised from their definitions in the same file.

Calls through vtables (`call_virtual`, `jalr`) have no constant target and are skipped.

## Output and use

Each finding prints `FILE: func_ADDR -> 0xT: asm sets a1 (callee reads it), C passes 1`, or `... reads v1 after a v0-only call`. The last line prints the totals. The exit code is 1 when there are findings.

The audit is a lint, not a gate. The analysis is linear in address order and approximate, so a hit is a review item: it is checked against the asm and fixed in the C, or explained. Once the existing hits are triaged, the audit runs before every batch PR, next to `frame_audit` (decomp workflow).

## Tests

`tools/tests/test_call_audit.py` builds small asm word sequences and C snippets for:
- both rules;
- the window boundary;
- a callee that passes a register through to its own callee;
- a body that assigns the register itself;
- a helper that keeps v1.

On the real dump, the audit must flag the two pre-fix bodies above.

**Sabotage:** dropping the rule must make those tests fail.
