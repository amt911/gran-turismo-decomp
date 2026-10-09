# Phase 2a — Recompiled GT Boots Headless Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `gt_native` — GT's whole EBOOT recompiled by the lcs-recomp framework, linked with a headless HLE host — boots and reproduces PPSSPP's first 2,000 import calls, running 30 s without a crash or `rt.unsupported`.

**Architecture:** setup.sh pins lcs-recomp and applies our codegen patches; `tools/recomp.py` regenerates `recomp/generated/` (ignored) from the verified ELF; a root CMake builds framework core + generated units + `host/` (HLE ported from lcs-recomp's MIT profile, LCS specifics removed); an import-call trace is compared with PPSSPPHeadless's log by `tools/trace_compare.py`.

**Tech Stack:** C++20, clang, CMake + Ninja, lcs-recomp `ab91e87`, Python 3.13 + pytest, PPSSPPHeadless 1.20.4.

**Spec:** `docs/superpowers/specs/2026-10-09-phase2a-recomp-boot-design.md`

## Global Constraints

- Generated C++ and anything else derived from the EBOOT stays in `recomp/` / `extracted/` (gitignored). Never print generated code into docs, commits or PRs.
- Framework pin: lcs-recomp `ab91e87fe38f3efc5e4443069e7ce6b5a43e615b`; our changes only as `tools/patches/lcs-recomp/NNNN-*.patch`.
- Load base `0x08804000`. ELF SHA1 `8746583fa8298f6447132988a01b236a8c5994b0`.
- Every build/run of generated code inside `systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0`, with `-j` from the measured per-unit peak.
- Code taken from lcs-recomp keeps its MIT notice at the top of the file and is listed in `docs/agents/toolchain.md`.
- Plans carry contracts (names, signatures, behaviour) for C++ not yet compiled; literal code only where it was run.
- Commits: Conventional Commits + `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.

## Review Focus

- **A stale `recomp/generated/` from an older ELF or older recompiler** — `recomp.py` must regenerate, not reuse; tested in Task 3.
- **A patch that already applied** (setup.sh rerun) — must be detected and skipped, not fail; checked in Task 1 by running twice.
- **PPSSPP log lines that are not import calls** (thread switches, debug lines) — `trace_compare.py` must ignore them; tested in Task 6.
- **Imports logged by NID only on one side and by name on the other** — normalise to `library::nid`; tested in Task 6.
- **The first divergence appearing after a long equal prefix** — report index, both calls and the 3 preceding calls; tested in Task 6.

---

### Task 1: setup.sh pins lcs-recomp and applies patches

**Files:** Modify `tools/setup.sh`, `docs/agents/toolchain.md`; Create `tools/patches/lcs-recomp/.gitkeep`.

**Interfaces:** Produces `tools/src/lcs-recomp` at the pin with every `tools/patches/lcs-recomp/*.patch` applied, and `tools/src/lcs-recomp/out/tools/{psp_analyze,psp_recomp,psprecomp_tests}` built (Ninja, clang, Release, `--target` only those three, inside `capped`).

- [ ] Replace the PSPRecomp checkout with lcs-recomp at the pin (PSPRecomp is no longer needed). Apply patches in name order: skip a patch when `git apply --reverse --check` succeeds (already applied), apply when `git apply --check` succeeds, otherwise stop naming it.
- [ ] Build the three targets; run `psprecomp_tests`.
- [ ] Run `tools/setup.sh` twice; second run builds nothing new and applies nothing.
- [ ] toolchain.md: replace the PSPRecomp row with lcs-recomp (pin, why, patches dir). Commit.

### Task 2: Codegen patches until every generated unit compiles

**Files:** Create `tools/patches/lcs-recomp/0001-*.patch`, `0002-*.patch` (made from edits in `tools/src/lcs-recomp`, exported with `git diff`).

- [ ] Root-cause `local_pc` undeclared: find which units get `jr` lowering without the dense dispatcher prologue that declares it (`tools/codegen_main.cpp` ~943 vs ~1116); fix so the declaration exists wherever `local_pc` is used. Export `0001`.
- [ ] Root-cause the VFPU scalar register `188` (`allegrex_context.hpp` limit 144): find the instruction kind and operand decoding that produce it (decoder vs emitter), fix the root cause, not the assert. Add a framework regression test in the existing `tests/` style if the decoder is at fault. Export `0002`.
- [ ] Regenerate; `clang++ -fsyntax-only` every unit (cgroup, `-j` 4) → 0 errors. Record any further defects as further patches in this task. Commit patches.

### Task 3: `tools/recomp.py` (TDD)

**Files:** Create `tools/recomp.py`, `tools/tests/test_recomp.py`.

**Interfaces:** `regenerate(elf: Path, out_dir: Path, recomp_bin: Path, load_base: int, runner=run) -> bool` (True when it ran). Stamp file `out_dir/.inputs.sha1` = sha1 of (ELF bytes + recomp binary bytes + load base). Skips when stamp matches and `generated_registry.cpp` exists. CLI: `.venv/bin/python -m tools.recomp`.

- [ ] Tests first: runs when no stamp; skips when stamp matches; reruns when ELF changes; reruns when binary changes; missing ELF → error naming `tools.extract`; missing binary → error naming `tools/setup.sh`. Watch them fail, implement, pass, commit.

### Task 4: `tools/extract.py --full` (TDD)

**Files:** Modify `tools/extract.py`, `tools/tests/test_extract.py`.

- [ ] Test first: with `full=True` the 7z command has no `-x!` exclusion; default unchanged. Implement `extract(..., full: bool = False)` and `--full`. An existing extraction without GT.VOL is completed when `--full` is requested (rerun 7z even if the ELF hash already matches). Run real: `extracted/iso/PSP_GAME/USRDIR/GT.VOL` exists (1,053,487,104 B). Commit.

### Task 5: Root CMake + headless host → `gt_native` links

**Files:** Create `CMakeLists.txt`, `host/main.cpp`, `host/gt_profile.cpp`, `host/gt_profile.hpp`, `host/README.md`.

**Interfaces:** `gt::install_profile(psprecomp::Runtime&, std::uint32_t user_arena_start)` registers all HLE. `main` mirrors lcs-recomp `lcs/host/main.cpp` (load+relocate at `0x08804000`, `register_generated_functions`, install profile, `$gp` from module info, `run(entry)`), minus GPU/audio/window; flags `--game <extracted/iso>`, `--elf`, `--max-seconds`, `--trace <file>`.

- [ ] CMake: `psprecomp_core` from the pinned framework's `src/*.cpp`; `gt_generated` from `recomp/generated/*.cpp` at `-O1`; `gt_native` = host + both. Fail configure with a clear message if `recomp/generated/` is missing.
- [ ] `gt_profile.cpp`: port lcs-recomp `lcs/host/lcs_profile.cpp` keeping its MIT header + a provenance line; delete LCS-only parts (address-specific patches, widescreen/menu/FPS hooks, renderer calls → no-ops); keep kernel/IO/UMD/power/utility/audio/MPEG/GE/display HLE as headless.
- [ ] Measure: compile the largest unit alone in the cgroup with `systemd-run --user --wait -p MemoryMax=6G` and record peak; pick `-j = floor(5 GB / peak)`. Build everything; link succeeds. Commit.

### Task 6: Import-call trace + `tools/trace_compare.py` (TDD)

**Files:** Modify `host/gt_profile.cpp` (trace hook); Create `tools/trace_compare.py`, `tools/tests/test_trace_compare.py`.

**Interfaces:** host writes one line per import call: `<index> <library>::<nid8hex>`. `parse_ppsspp_log(path) -> list[str]`, `parse_gt_trace(path) -> list[str]`, `first_divergence(a, b) -> Divergence | None` (index, a_call, b_call, context: 3 previous). CLI prints `match N calls` or the divergence.

- [ ] Tests first (synthetic logs): PPSSPP lines `N=sceKernelCreateThread(...)` style normalised via a NID map file (`library,nid,name` from lcs-recomp `configs/nids.csv`); non-call lines ignored; equal prefix → `None`; divergence after long prefix reports index + context. Implement, commit.
- [ ] Hook: wrap HLE dispatch (in `gt_profile` registration helper) to append to the trace when `--trace` is given.

### Task 7: Boot to the gate

- [ ] Reference: `PPSSPPHeadless "<iso>" -l --timeout=30 --graphics=software > recomp/analysis/ppsspp.log` (disk, not /tmp).
- [ ] Run `gt_native --game extracted/iso --trace recomp/analysis/gt.trace --max-seconds 30` (cgroup). Compare. Iterate: each divergence/crash → `systemd-run --wait`-less debugging per `docs/agents/debugging.md` (instrument first), fix HLE or add a codegen patch, record non-obvious causes in FINDINGS.
- [ ] Gate: ≥ 2,000 equal calls, 30 s, no crash, no `rt.unsupported`. Show the gate failing once (disable one HLE) before trusting it.

### Task 8: Docs, PR, verification, merge

- [ ] AGENTS.md *Commands* (recomp/build/run/compare), FACTS (numbers), ROADMAP (Fase 2a ✓, 2b next), toolchain.md. Lint. PR with manual test plan; verification in a clean worktree (setup → extract --full → recomp → build → gate); comment verdict; merge (authorised in this mode).
