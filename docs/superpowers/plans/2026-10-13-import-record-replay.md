# Import record & replay — implementation plan

Spec: `docs/superpowers/specs/2026-10-13-import-record-replay-design.md`. Executed inline (TDD), one
commit per task.

1. **`ImportLog` (pure, TDD).** `host/import_replay.{hpp,cpp}` + `host/tests/import_replay_test.cpp`
   (CTest `import_replay`).
   - `ImportRecord { stub, arity, args[8], changed registers (index, value), RAM runs (address, bytes),
     KernelReplayState before/after }`.
   - `record_ram_changes(before, after, base)`: changed byte runs between two RAM images, compared by
     4 KiB page.
   - `ImportLog::next(stub, args) -> Match{ok, record*, error}`: stub, arity-limited arguments, a past-end
     call.
   - `apply_registers` and `apply_ram`; `leftover()` with the count and the first unconsumed record.
2. **Import gate in the harness.** `diff_harness.cpp`: install a gate on every stub in diff mode
   (`set_import_stubs` gains NID, name and recordable flag). Phase Normal, Recording or Replaying.
   Recording runs the HLE and appends; Replaying matches and applies. `run_to_sentinel` treats
   recordable stubs like replay-safe. A switch during a recorded call stops the run with a message.
   Mismatches: the replay error, leftovers, and the kernel state before the call.
3. **Continue live on abort.** Save the registers before each import in a case. On abort, don't
   restore: continue from the stop point, or from the pre-import registers when isolation broke. A
   handler at `kSentinelReturn` jumps to the saved return address; no new cases while one is pending.
4. **Recordable list + names.** `host/main.cpp`: recordable NIDs with arity and a reason; import names
   in the report; a per-import summary of not-comparable cases.
5. **sceKernelWakeupThread** under isolation, if the thread table is reachable from gt_kernel.cpp:
   refuse and break isolation when it would preempt.
6. **Verify.**
   - Full gate: no regressions, with not-comparable counts before and after.
   - Re-register the batch 56–63 "can't replay" functions with GT_DECOMP_ONLY, then sabotage them.
   - Prove the new mismatch checks fail when they should.
   - Docs: FINDINGS, ROADMAP, decomp-workflow note. Then the PR.
