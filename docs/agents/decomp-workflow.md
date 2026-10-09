# Decomp workflow — from asm to readable C that behaves the same

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

**Status:** this loop becomes runnable once the recompiled baseline (Phase 2) and the differential
harness (Phase 3) exist. Until then it is the contract those phases have to satisfy, not a recipe that
works today. Commands marked *(planned)* don't exist yet.

## Why differential, not matching

Strategy B (user, 2026-10-09): the exact Sony GCC builds are not public, and the goal is source that
compiles on other platforms. So a decompiled function is proven by **behaving exactly like the
original** on the same inputs — the original being the PSPRecomp translation of the real code, which is
mechanical and therefore trusted. Byte-matching survives only as optional extra evidence (last section).

## The loop, per function

1. **Pick** — from the progress report *(planned)*: leaves first (no undecompiled callees needed),
   functions whose callers or callees are already done, library code identified by NID/string. Claim it
   in the branch name so two agents don't take the same function.
2. **Context** — collect what's known: `include/` types, `config/symbol_addrs.txt`, FACTS, REA/Ghidra
   pseudocode of the function and its callers ([reverse engineering](reverse-engineering.md)).
3. **Draft** — m2c (`-t mipsel-gcc-c`) and Ghidra pseudocode are **reading aids**. The C/C++ in `src/`
   is written by hand, with real names, types and structure. PSPRecomp's generated C++ is never the
   draft (`AGENTS.md` § Decompilation).
4. **Capture cases** *(planned)* — the harness records the function's inputs (argument registers, the
   guest memory it reads) and outputs (return value, memory it writes, calls it makes) from real runs
   of the recompiled game reaching the states that exercise it. Cases hold no game bytes beyond what
   the function touches, and live in `tests/differential/` only if they are free of copyrighted
   content; otherwise they are regenerated locally from the user's dump.
5. **Run differential** *(planned)* — recompiled original vs decompiled, same cases, everything
   compared. Iterate until equal.
6. **Boot check** — the full game with the function swapped reaches the reference states
   ([runtime verification](runtime-verification.md)).
7. **Land** — swap the function to the decompiled version in the default build, name what you can
   prove, commit with the case count. Stuck past the [debugging budget](debugging.md#the-loop-is-the-cost):
   leave it recompiled, keep the draft in a branch or behind a disabled switch with a one-line note.

## What "done" means here

- The differential cases pass **and** they cover the function's branches that real play reaches —
  a function tested only on its early-return path is not done. Record which branches the cases hit.
- **A function that writes memory needs at least one compared case that changed memory**
  (`cases that changed memory` in the `[decomp]` report). A memset of already-zero memory proves
  nothing about the writes: a sabotaged memset passed such cases (FINDINGS).
- **The gate is shown failing once per function**: sabotage the return value or a write, run
  `--decomp diff`, see `mismatched` > 0 and exit code 2, restore.
- The boot check is green with the function swapped in.
- The code compiles cleanly with the host compiler at the project's warning level — no new warnings.

## Calling other guest functions from decompiled code

`gt::call_guest(rt, ctx, address, {args…})` (`host/guest_call.hpp`) runs a recompiled — or already
decompiled — guest function as a nested call and returns its `v0`. Rules:

- **The callee must not block in the kernel** (switch PSP threads). A native C++ frame can't be
  suspended the way recompiled code unwinds to the dispatcher, so a switch stops the runtime with a
  message naming the call. Uncontended LwMutex locks are fine; anything that waits is not.
- Arguments go in `a0-a3, t0-t3`; the callee's frame goes below the caller's `sp`. The caller keeps
  its own state in C++ and gets only `v0` back.
- The harness still compares such functions: an isolated original may call the **replay-safe
  imports** (`gt::set_replay_safe_imports`: LwMutex lock/unlock, interrupt suspend/resume, data-cache
  maintenance) because their state lives in guest RAM, which the harness snapshots and restores.
  Any other import, or a thread switch, makes the case "not comparable".
- A function the reached game states never call can't be compared, so it stays recompiled (e.g.
  `calloc`/`realloc`, `src/libc/malloc.cpp`).

## Interop with guest memory — open, decided in the Phase 3 spec

Decompiled code runs next to recompiled code that keeps every game object in **guest memory** (32-bit
PSP addresses; PSPRecomp has `guest_memory.hpp`). Decompiled code must read and write that same memory
until the last recompiled function is gone. The options (typed guest-pointer wrappers, a fixed low
host mapping with 32-bit pointers, a 32-bit host build) each trade readability, portability and speed
— the spec chooses, with numbers. Until then, don't write decompiled code that assumes host pointers.

## GCC/Allegrex specifics to keep in mind

Expectations to confirm per function, not established facts about this binary:

- **`-G0`** is a fact (FACTS): globals are `lui`/`addiu` pairs, no `$gp` data.
- **EABI32 + `long32`** (FACTS: `e_flags`, `.gcc_compiled_long32`): EABI passes more arguments in
  registers than O32 (`$a0–$a3` plus `$t0–$t3`) — check m2c's assumptions against the asm.
- **Multiplication by constants can appear as shifts/adds** (seen once so far: `x * 148` in
  `func_00013DD0`, FACTS) — read such sequences back as `x * N` in C.
- **C++**: vtables live in `.rodata`; `.ctors` (334 entries) lists one static initializer per
  translation unit that has globals with constructors — evidence for **TU boundaries**.
- **VFPU** (0.18 % of instructions, concentrated in `0x300000–0x3fffff`): decompiled as portable math
  (plain C or the host's SIMD behind a helper in `include/`), never as inline VFPU.
- **Statically linked SDK code** (libc, libstdc++, SGX) is identified first; much of it can be replaced
  by the host's own libc/libstdc++ instead of being decompiled — progress for it is reported
  separately.

## Optional: a matching check

`psp-gcc-1.7.1` (GCC 3.3.3 + Allegrex 2.2.2, on decomp.me and in `tools/compilers/`) reproduces some
game functions byte for byte with `-O2 -G0` (FACTS). When a decompiled function also matches with it,
say so in the commit — it is strong extra evidence. A non-match proves nothing (the real compiler
differs: different multiplication costs, for one) and never blocks a function.

## Progress is reported honestly

- Two numbers, never merged: **game code** and **SDK/library code**, each as decompiled-and-passing
  functions / total, and bytes of `.text` they cover.
- Functions that only run recompiled count as not decompiled, however well the game runs.
- Progress claims in a PR come with the report output that produced them.
