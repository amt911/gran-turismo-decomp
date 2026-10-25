# Synthetic cases Implementation Plan

> **For agentic workers:** executed inline (low-token mode; the user approved spec and plan in advance on 2026-10-25).

**Goal:** verify decompiled functions no pad route reaches, by running harness cases with generated inputs.

**Spec:** docs/superpowers/specs/2026-10-25-synthetic-cases-design.md

## Global Constraints

- Heavy runs inside `gt-heavy.slice`; real-time runs under `tools/cpu_lock.sh`; large dumps never in tmpfs.
- The route gate (fast/full, floor) is unchanged; synthetic results are reported apart.
- `tools/` is TDD with pytest, coverage ≥ 80 %; host code gets a CTest unit test.

## Review Focus

- A fault in the original must never count as a mismatch; a fault only in the decompiled run must.
- After every synthetic case the RAM the case wrote is restored (no case sees an earlier case's writes).
- Route cases stay off under GT_SYNTH, so the report counts synthetic cases only.
- The sabotage over synthetic cases must actually fail a function (sabotage the check).

### Task 1: input generator
Files: `host/synth_case.hpp`, `host/synth_case.cpp`, `host/tests/synth_case_test.cpp`, CMake target `synth_case_tests`.
`SynthInputs make_synth_case(std::uint32_t function, unsigned index, std::uint64_t seed, const SynthLayout &layout)`:
returns 8 integer argument registers, 8 float registers (bits) and the arena words. Tests from the spec, written first.

### Task 2: harness and trigger
Files: `host/diff_harness.{hpp,cpp}`, `host/gt_kernel.cpp` (ctrl HLE hook), `host/main.cpp` if the report needs it.
`run_synthetic_cases(rt, ctx)`: for each installed function, N cases through the trampoline's case path with
`g_synthetic` set (always sampled; an original that does not finish or throws is skipped and never continues live; the
pages are restored after the comparison instead of keeping the original's result). Under GT_SYNTH live calls are never
sampled. Verified end to end in Task 4.

### Task 3: tools.decomp_gate synth / sabotage --synth
Files: `tools/decomp_gate.py`, `tools/tests/test_decomp_gate.py`. `synth TAG --only ... [--route race] [--cases N]`;
`sabotage ... --synth`. Tests for the command line built and the report read.

### Task 4: end to end and docs
Run synth on a known route-verified function (expect equal), on the same function sabotaged (expect a mismatch), then
on the never-compared decompiled functions. Docs: FINDINGS (what synthetic cases can and cannot show), decomp-workflow
(the synthetic category), ROADMAP, README count line.
