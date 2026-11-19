# Emit unreferenced functions, Phase A — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkboxes.

**Goal:** let `psp_recomp` emit functions nothing in the code or data reaches, starting with the 370 that have reference evidence (Phase A).
**Architecture:** a new patch (0073) adds a seeds file (7th `--auto` argument) whose addresses become `extra_seed` seeds; `tools.recomp` passes it and hashes it into its stamp; `tools/extra_seeds.py` selects the seeds (classification and evidence filter) and verifies a regeneration against the one before it.
**Tech Stack:** C++ patch to the pinned lcs-recomp tree, Python 3 + pytest (coverage >= 80 %).
**Spec:** `docs/superpowers/specs/2026-11-19-emit-unreferenced-functions-design.md`

## Global Constraints
- A separate seeds file, not the 0014 boundaries file (spec § 1). Seeds are emitted as ordinary code.
- `functions.tsv` is an input only; it must stay byte-identical (spec § 2).
- No game bytes in git: `recomp/extra_seeds.txt` is derived locally and ignored.
- Phase A = data-pointed 246 + called-from-unemitted 80 + called-from-unreached-block 34 + lui-in-unreached-block 10, after the § 4 filter.
- Proof (all four, output in the PR): generated-set superset + tsv identical; trace identical; full gate same; synthetic counts of existing registrations unchanged.

## Review Focus
- A seed inside an existing function (split) must fail verification.
- An entry the previous function falls into (shared tail / wrong Ghidra boundary) must be rejected by the filter.
- An empty or missing seeds file must leave the recompiler input digest and output unchanged.
- A seed outside executable ranges is ignored by the analysis (add_seed), not an error.
- A seeds file line with a comment or blank line parses.

### Task 1: patch 0073 and `tools.recomp --extra-seeds`
**Files:** Create `tools/patches/lcs-recomp/0073-codegen-extra-seeds.patch`; Modify `tools/recomp.py`; Test `tools/tests/test_recomp.py`.
**Interfaces:** Produces `regenerate(..., seeds: Iterable[int] = ())`; with seeds, the command gains the boundaries slot (empty string when no boundaries) and the seeds file path `out/extra_seeds.txt` as arguments 6 and 7; the stamp digest includes the seeds listing.
- [ ] Tests: `test_seeds_are_passed_as_seventh_argument`, `test_changing_seeds_reruns`, `test_no_seeds_leaves_command_unchanged` (assert the existing command list is identical).
- [ ] Implement; the patch adds `extra_seeds` to `analyze_program` and the argument to `generate_auto`.
- [ ] Verify the series applies from scratch: `tools/setup.sh` and `psp_recomp` rebuilds.

### Task 2: `tools/extra_seeds.py` selection
**Files:** Create `tools/extra_seeds.py`, `tools/tests/test_extra_seeds.py`.
**Interfaces:** `references(words, relocs, base, elf_words, functions) -> dict[int, Evidence]`; `falls_into(words, base, start, previous) -> bool`; `select(functions, emitted, image, elf, phase) -> (seeds, rejected)`; `write_seeds(path, seeds)`.
- [ ] Tests with a synthetic `.text`: a called-only-from-unemitted function is selected; a function the previous one falls into is rejected with reason `shared-tail`; a function with no reference and no prologue is rejected `no-evidence`; phase A excludes the no-reference class.
- [ ] Implement.

### Task 3: `verify` and the sabotage
**Files:** Modify `tools/extra_seeds.py`; Test `tools/tests/test_extra_seeds.py`.
**Interfaces:** `verify(before_dir, after_dir, tsv_before, tsv_after, seeds, functions) -> list[str]` (empty = pass).
- [ ] Tests: passes for a superset; fails when a registered address disappears; fails when the tsv differs; **sabotage**: a seed inside another function's body fails with `splits`.
- [ ] Implement and the CLI (`select`, `verify`).

### Task 4: real regeneration, the four proofs, PR
- [ ] Baseline (done before the patch): `recomp/generated_before`, build, trace, full gate, synthetic counts.
- [ ] Seeds, regenerate, `verify`, build, trace, full gate, synthetic counts; compare; PR with outputs.
