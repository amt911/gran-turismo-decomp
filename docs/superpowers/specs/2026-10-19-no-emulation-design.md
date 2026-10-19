# No emulation: from the recompiled baseline to a native port

Status: **draft for the user's approval** (asked for on 2026-10-15, "quitar toda la capa de emulación y dejarlo
nativo"; AGENTS.md § End state; ROADMAP Fase 5). No code before approval.

## Where we are (2026-10-19)

- **Code:** 6,622 of 23,208 functions are decompiled (28 %), gated against the recompiled original. ~430 more are
  reached by the five pad routes; the rest are reached by no route (menus, modes and cars the routes never visit) or
  are Sony SDK code.
- **Memory:** all game objects live in a 32 MB `GuestMemory` arena at PSP addresses, shared by recompiled and
  decompiled code. Decompiled code reads it through `word(rt, x + 0x30)` or, in the named subsystems, through typed
  views (`gt::guest::View<T>`, `GT_FIELD`, `Pointer<T>`: `include/gt/guest.hpp`, spec 2026-10-13).
- **Calls:** a decompiled function calls any other function through `gt::call_guest`, which dispatches by guest
  address, decompiled or not, and goes through the runtime's scheduler (PSP threads, fibers).
- **Platform:** the Sony libraries are HLE functions in C++ (`host/gt_kernel.cpp`, lcs-recomp's host); the GE is a
  command-list decoder feeding a Vulkan backend (patches 0019-0051).

"Emulation" today is therefore four things: the recompiled code, the guest memory arena, the guest calling convention
(registers, `call_guest`, the PSP thread scheduler), and the PSP's hardware interfaces (GE command lists, sceAudio,
sceCtrl, the IO kernel).

## The end state

A native program: C++ source for all of GT's code, game objects as C++ types with host pointers, functions calling
each other directly, threads as host threads (or a cooperative scheduler of the game's own), and a platform layer
(window, Vulkan renderer, audio, input, files) with no PSP shape. Same behaviour as the original: a race, a menu
and a save must come out the same.

## Stages (each keeps a verified reference)

The differential gate compares a decompiled function with the recompiled one on the same guest memory. It can only
exist while both share that memory and calling convention. So the stages are ordered to keep a reference at every step,
and the reference changes kind as the emulation goes away.

### Stage 1 — all of GT's code decompiled (memory and calls unchanged)

- **What:** batches continue until every function the game runs is decompiled.
  - Routes come first: new pad routes make more code reachable and therefore verifiable (lessons: more routes beat
    more code).
  - Then the code no route reaches. It can only be checked against the asm by hand, so it is marked as such.
  - Sony SDK code (libc, libstdc++, sce* stubs, SGX) is replaced by the host's equivalents, not decompiled.
- **Reference:** the differential gate (unchanged) plus the boot and GPU gates.
- **Done when:** `gt_native --decomp on` runs every route with the same import trace as `--decomp off`. This needs the
  deterministic-route work below, and then the recompiled code can be dropped.

### Stage 2 — direct calls between decompiled functions

- **What:** a call to a function that is decompiled becomes a C++ call (`func_08A63C5C(rt, ctx, ...)`, then a named
  function with typed arguments) instead of `call_guest(0x08A63C5C)`. Arguments become C++ parameters; the register
  context shrinks to what the remaining guest code needs.
- **Reference:** the differential gate still works if the gate build keeps the old call path.
  - A `GT_DIRECT_CALLS` build switch: off in the gate build, where every call goes through the trampolines and nested
    cases; on in the game build.
  - New check: the game build's import trace and frame output equal the gate build's on the same routes.
- **Why:** it is a step in itself (no guest calling convention left in decompiled code), it makes the code readable,
  and it is a speed gain. Every `call_guest` today costs a dispatch and a scheduler boundary.

### Stage 3 — typed memory, subsystem by subsystem

- **What:** every game struct gets a layout (the 2026-10-13 recipe) and code uses views, not offsets. Then views
  become plain C++ objects.
  - While recompiled code exists (stage 1 not finished), objects must stay in the arena at the PSP layout.
  - Once stage 1 is done, a subsystem's objects can move to host memory with host pointers, behind accessors, one
    subsystem at a time.
  - Data that comes from GT.VOL or the save keeps its PSP layout on disk. It is loaded through explicit (de)serialisers
    instead of being used in place.
- **Reference:** game-level checks, since memory equality between two layouts is no longer defined.
  - The import trace (file reads, sounds started, pad reads).
  - The save files written by the routes, byte for byte.
  - The GPU gate frames.
  - Per-subsystem state dumps in a neutral format (for example a race's car positions per frame) compared before and
    after the move.

### Stage 4 — the platform layer

- **Renderer.** GT builds GE command lists through statically linked graphics-library code (sceGu-like; not yet
  identified in our symbols, to be located first). Once that code is decompiled, it calls a renderer API (draw these
  vertices with this state) instead of writing command words. The GE decoder then leaves the frame path.
  - This is also the next large performance gain. FINDINGS 2026-10-19 shows the GE thread's cost is the decoding
    itself, spread over many small steps.
  - Reference: the GPU gate frames (the decoder path stays available for A/B).
- **Audio, input, files, time.** The HLE functions become the platform layer's API, called directly by the decompiled
  game. The PSP thread scheduler is replaced by host threads, or by a deterministic cooperative scheduler where the
  game relies on PSP ordering.
  - Reference: the import trace equivalence, then the route outcomes (saves, frames).

## Prerequisites found on the way

- **Deterministic routes.** `--decomp on` sometimes misses a menu press: routes press at fixed vblanks, and a menu
  that opens a few frames later misses one (FINDINGS 2026-10-19). Routes should wait for a state (a screen id or a
  frame checksum) before pressing. Stage 1's exit check and stage 3's references need this.
- **More routes.** Only code a route runs can be verified. Each new mode visited (licenses, arcade, other circuits,
  the car dealer's other makers) adds reachable code.

## Decisions for the user

1. **Order.** Recommended: 1 (ongoing) → the deterministic routes → 2 → 3 by subsystem → 4. Stage 4's renderer part
   could start earlier for performance, as soon as the graphics-library code is located and decompiled, because the
   GPU gate can verify it on its own.
2. **Scheduler.** Keep a deterministic cooperative scheduler, which is faithful and easy to verify, or move to host
   threads, which is more native and harder to verify. Recommended: cooperative first.
3. **SDK code.** Replace it with host equivalents, not decompile it. Recommended: yes.

## Out of scope here

Mods (they follow from stage 3), other platforms (they follow from stage 4), and byte matching.
