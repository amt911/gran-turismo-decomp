# Nested verification — design (2026-10-13)

**Problem.** Inside a differential case, nested calls to other decompiled functions ran their
*originals* in both runs. A decompiled function that the routes reach only from inside its decompiled
callers (accessors, helpers of a subsystem) was therefore never executed by any case: `calls 0`,
unverifiable, and it stayed unregistered. Measured: enabling the 129 written-but-unregistered subsystem
functions and adding the buy route compared only 8 of them.

**Change.**

1. **Harness (`host/diff_harness.cpp`).** In a case's *decompiled* run (`ImportPhase::Replaying`),
   nested decompiled callees run their decompiled versions; the original's run keeps running originals.
   The case then compares the whole decompiled call tree against the whole original one. Imports inside
   nested callees replay from the original's log as before, so a callee that calls a different import
   sequence is a mismatch.
2. **Accounting.** Each function counts the cases it ran nested in, by outcome (`nested in N equal / M
   mismatched cases` on its report line). A function with `compared 0` but `nested_equal > 0` was
   reached, and the "never compared" warning no longer fires for it.
3. **Attribution.** With callees running decompiled, a parent's mismatch can be its callee's fault. The
   harness prints each distinct mismatching case as `[decomp] mismatch case: 0xPARENT nested 0xA,0xB`
   (or `nested -`, at most 4096). `tools.decomp_gate sabotage` pins a detection on a function only when
   it was the **only sabotaged function** in some mismatching case. Functions left ambiguous (a parent
   and its callee always mismatching together) are re-sabotaged in further rounds, choosing per round
   functions that never shared a mismatching case (up to 4 rounds).
4. **Sabotage that survives callers.** The sabotage also bumps a kernel-region RAM word: a caller that
   saves and restores s0 erases an s0 bump in a function reached only nested (FINDINGS).

5. **Opt-in per callee (`GT_DECOMP_NESTED`).** Running *every* decompiled callee nested made the full gate
   report 1131 mismatching functions. They were not wrong behaviour of the parents but wrong **return
   contracts** of callees: a function registered `Void` whose original leaves a value in v0 that callers
   read (`func_08987A74` returns the state it stores; its caller reads it as the token), or a parent that
   passes a `Void` callee's leftover v0 on as its own result. A top-level case never compares v0 of a
   `Void` function, so those registrations passed. Only callees registered with `GT_DECOMP_NESTED` run
   decompiled when nested; the subsystem registrations use it. `tools.contract_audit` checks the claim
   before every gate: no direct caller (recompiled code) and no decompiled caller (`call_guest` result)
   reads a return register the registration leaves uncompared — a lower bound (reads after a branch or
   through indirect calls aren't seen). The generic registrations join once their contracts pass it.

**Verified, restated.** Reached (compared > 0, or nested in > 0 equal cases) with 0 mismatches in the
fast or full gate, **and** a sabotage pinned on it by the tool.

**What it does not change.** The game still continues with the original's result; a case still never
opens a case inside itself; non-decompiled code is untouched.

**Risks, checked by the experiment before landing.** A nested decompiled callee that is not comparable
on its own (blocking imports, thread switches) can now make its parent's case not comparable — counted
by the existing `not comparable` totals, compared before and after on the same routes.

**Tests.** `tools/tests/test_decomp_gate.py` (parsing, attribution, rounds). The harness change is proven
by the experiment: a fast gate with every written subsystem function enabled, a sabotage of children
reached only nested (pinned by the tool), and the full gate with 0 mismatches.
