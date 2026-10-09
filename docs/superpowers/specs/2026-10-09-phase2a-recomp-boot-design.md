# Phase 2a — recompiled GT compiles, links and boots headless (design)

Written in "modo desatendido" on 2026-10-09; decisions are the agent's, listed under *Decisions* with
their cost if wrong. Phase 2 was split: **2a** (this spec) gets the whole EBOOT recompiled, built and
running far enough to be compared with PPSSPP at the import-call level, with no graphics, audio or input.
**2b** (own spec, later) adds the Vulkan GE renderer, SDL window, audio and input.

Reminder of the goal: the recompiled build is **scaffolding for the decomp** (and later mods); it is
never the product, and its generated C++ never enters git.

## What the exploration established (FACTS has the numbers)

- PSPRecomp's framework builds on Linux/clang; its generic analyzer reads GT completely (243,383
  relocations, 0 unsupported; 41,212 function seeds; 329 imports).
- PSPRecomp's generic `psp_recomp --auto` has an endless-emission bug on `jal` to an import stub
  (`continue` without advancing `pc`) and emits calls to non-existent units. **lcs-recomp's copy of the
  same framework already fixes both** and has Linux-tested build glue. With it, GT generates 39 units /
  117 MB of C++ in ~30 s.
- Two codegen defects remain on GT's output, both absent from LCS: a VFPU register index `188`
  emitted where the context holds 144, and `local_pc` used by `jr` in units that never declared it.
- The framework does not lower VFPU in the generic path in all cases; GT has 2,164 VFPU instructions.
- PPSSPPHeadless boots GT into its intro movie with 0 unimplemented syscalls and logs every HLE call —
  a free reference trace.

## Decisions

1. **Base framework = lcs-recomp @ `ab91e87fe38f3efc5e4443069e7ce6b5a43e615b`** (MIT), not upstream
   PSPRecomp — it carries the fixes GT needs and builds on Linux. Fetched by `tools/setup.sh` into
   `tools/src/lcs-recomp`, never vendored. *Cost if wrong: switching back means re-applying our patches
   to another tree.*
2. **Our fixes to the framework are patch files** in `tools/patches/lcs-recomp/NNNN-*.patch`, applied by
   `setup.sh` after checkout (idempotent: `git apply --check` first). Each is minimal and upstreamable;
   proposing them upstream is the user's call. *Cost: a pin bump may need patch refresh.*
3. **Our build does not use lcs-recomp's CMake** (it always builds the LCS profile and needs Vulkan).
   A root `CMakeLists.txt` in this repo compiles the framework sources it needs (`src/*.cpp` minus
   anything LCS-specific) into `psprecomp_core`, the generated units from `recomp/generated/`, and our
   host from `host/`, into `gt_native`. *Cost: tracking framework file-list changes on pin bumps.*
4. **Generated units are compiled at `-O1`** by default with `-j` capped so the build fits the 6 GB
   ceiling; the exact `-j` comes from measuring one unit's peak RSS (spec acceptance records it).
5. **Host for 2a = headless HLE layer written for GT** in `host/`: kernel (threads, semaphores,
   event flags, callbacks, memory), IO (`disc0:` → `extracted/iso`, `ms0:` → a gitignored
   `savedata/` dir), UMD, power, utility (stubbed dialogs), display/GE accepting lists without
   drawing, audio/SAS/ATRAC/MPEG accepting calls and returning what PPSSPP's trace shows. Anything not
   implemented logs `UNIMPLEMENTED lib::nid` once and returns 0. Implementations are written from
   observed behaviour and public docs; code copied from lcs-recomp/PSPRecomp hosts (MIT) keeps its
   notice in the file header.
6. **`GT.VOL` is needed at runtime**: `tools/extract.py --full` extracts the whole ISO (adds ~1 GB to
   `extracted/`, gitignored); the default stays without it.
7. **The 2a gate is an import-call trace compared with PPSSPP's**: both runs log the ordered sequence
   of imported calls (`library::name` or NID) for the first N calls; `tools/trace_compare.py` reports
   the first divergence. Acceptance: the recompiled build reproduces PPSSPP's sequence for the first
   **2,000** calls (covers boot, thread creation, file opens of `GT.VOL`, start of the intro movie),
   and runs 30 s without a crash or `rt.unsupported` hit.

## Components

| Unit | Responsibility | Tests |
| --- | --- | --- |
| `tools/patches/lcs-recomp/0001-…` | codegen: declare `local_pc` in every unit that can emit `jr` | rebuild + all 39 units compile |
| `tools/patches/lcs-recomp/0002-…` | codegen: VFPU register index — fix the encoding that produced `188` (root cause first) | unit test in the framework test style + the unit compiles |
| `tools/setup.sh` | fetch lcs-recomp at the pin, apply patches, build `psp_analyze`/`psp_recomp` | idempotent rerun |
| `tools/recomp.py` | run `psp_recomp --auto` on the verified ELF into `recomp/generated/`, skip if inputs unchanged (hash of ELF + tool binary) | pytest with a fake runner |
| `CMakeLists.txt` + `host/` | `gt_native` executable: framework core + generated units + HLE host | builds; boots |
| `host/trace.*` | ordered import-call trace to a file | exercised by the gate |
| `tools/trace_compare.py` | parse PPSSPP `-l` log and our trace, normalise names, report first divergence | pytest with synthetic logs |

## Error handling

- Missing inputs (no `extracted/EBOOT.elf`, no generated units) → the tool says which command to run.
- Patch that no longer applies → `setup.sh` stops naming the patch.
- Unimplemented import → logged once with library and NID, returns 0, counted in an exit summary.
- `rt.unsupported` (instruction not lowered) → logged with address and mnemonic; the 2a gate fails if
  any is hit within the 30 s window.

## Testing

- pytest for `recomp.py` and `trace_compare.py` (TDD, synthetic fixtures, no game bytes).
- Framework patches: the framework's own test binary must stay green; add a regression test for each
  codegen fix where the framework's test style allows it.
- The gate itself (trace equality for 2,000 calls, 30 s run) is the integration check; it is shown
  failing once (e.g. by disabling one HLE function) before it is trusted.
- Memory: every build and run inside the 6 GB cgroup; one unit's peak compile RSS measured and recorded.

## Out of scope (2b and later)

Rendering (Vulkan GE backend, SDL window), audible audio, input, save data UI, networking, VFPU
correctness beyond what the 30 s boot window executes, performance. The differential harness for
decompiled functions is Phase 3.

## User actions batched (unattended mode)

- `sudo pacman -S vulkan-headers` — needed only for 2b (Vulkan), not for 2a.
