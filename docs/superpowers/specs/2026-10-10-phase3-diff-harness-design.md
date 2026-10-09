# Phase 3 — differential harness (design)

Written in "modo desatendido" on 2026-10-10 (user asleep, autonomy delegated: push, PR, merge). Decisions
are the agent's and list their cost if wrong.

## Goal

Prove each decompiled function behaves exactly like its recompiled original **inside our own
`gt_native` run**, so decompilation can start now, independently of PPSSPP parity (Phase 2a) and of
graphics (Phase 2b). End goal unchanged: readable, moddable source.

## How it works

1. A decompiled function lives in `src/` as `void name(psprecomp::Runtime &, psprecomp::AllegrexContext &)`
   and registers itself with `GT_DECOMP(0x08XXXXXX, name)` (runtime address).
2. `gt_native --decomp off|on|diff` (default `off` while the corpus is young):
   - `off`: everything runs recompiled.
   - `on`: each decompiled function replaces its original (`Runtime::register_function`).
   - `diff`: for the first N calls of each decompiled function (`--decomp-cases`, default 16):
     snapshot (CPU context + 32 MiB RAM) → run the **original** in isolation to its return address →
     record result → restore snapshot → run the **decompiled** version → compare → restore the
     original's result so the game continues exactly as recompiled.
3. Compared: `v0`, `v1`, callee-saved `s0–s8`, `sp`, `fp`, `ra`, FPU `f0`, and every RAM byte.
4. A case is **not comparable** (counted, not failed) when the original calls an import (HLE side
   effects can't be replayed) or does not return to its caller within a step bound.
5. At exit: per-function report (compared / equal / mismatched / skipped). Any mismatch → exit code 2:
   this is the gate a decompiled function must pass.

## Decisions

- **Running the original in isolation** needs its function pointer after it has been replaced:
  framework patch `0011` adds `Runtime::function_at(address)` (public wrapper of `lookup_function`).
  A small dispatch loop (sentinel return address, bounded steps) follows calls into other units.
  *Cost if wrong: functions that tail-call through the dispatcher in odd ways are reported "not
  comparable" instead of compared.*
- **RAM only** in the snapshot (32 MiB at `0x08000000`); VRAM/EDRAM not compared in v1. *Cost: a
  function writing VRAM could hide a difference — none of the first targets do.*
- **Import observer is shared**: `host/main.cpp` installs one observer that both feeds the trace and
  counts imports for the harness.
- **First target: the boot `memset`** (vaddr `0x332018`, runtime `0x08B36018`), a library leaf called
  constantly during boot: proves the harness on many real cases. Decompiled from its behaviour, not by
  pasting Ghidra or recompiler output.

## Testing

- Unit tests (CTest, `host/tests/`) for the comparison logic, written first.
- Gate shown failing once: a deliberately wrong variant of the first function makes `diff` exit 2.
- Whole run inside the 6 GB cgroup; tool tests stay green.

## Out of scope

Functions that call imports (need an HLE-replay design), VRAM comparison, performance of `on` mode.
