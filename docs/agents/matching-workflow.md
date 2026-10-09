# Matching workflow — from asm to C that rebuilds the same bytes

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

**Status:** this loop becomes runnable at Phase 3 (checksum-green asm build) and needs the Phase 2
compiler. Until then it is the contract the tooling has to satisfy, not a recipe that works today.
Commands marked *(planned)* don't exist yet.

## The loop, per function

1. **Pick** — from `objdiff-cli report` *(planned)*: prefer leaves (no unknown callees), functions whose
   callers or callees are already matched, and library units identified by NID/string. Claim it in
   the PR/branch name so two agents don't take the same unit.
2. **Context** — collect what's known: `include/` types, `config/symbol_addrs.txt`, FACTS, REA/Ghidra
   pseudocode of the function and its callers ([reverse engineering](reverse-engineering.md)). Build a
   context file for m2c from the headers the unit includes.
3. **Draft** — `m2c -t mipsel-gcc-c --context <ctx.h> <func.s>` for C; for a C++ unit, use the m2c draft
   as a reading aid and write the C++ by hand. Ghidra pseudocode is a second opinion, never pasted.
4. **Compile and diff** — `objdiff-cli diff` *(planned)* on the unit. Iterate on types, statement order,
   temporaries, signedness, inline candidates — the usual GCC levers.
5. **Stuck near 100 %** — `decomp-permuter` inside the memory cgroup with `-j` capped; accept its result
   only if the C still reads like code a person wrote.
6. **Land** — matched: remove the `INCLUDE_ASM`, name what you can prove, commit with the objdiff number.
   Not matched after the [debugging budget](debugging.md#the-loop-is-the-cost): keep the C behind
   `NON_MATCHING` with a one-line note of the remaining diff, and move on.

## What "matched" means here

- objdiff reports **100 % for the function, including relocations** — a correct instruction with the
  wrong symbol in its relocation is not a match.
- `ninja check` *(planned)* stays green for the whole binary.
- Data and rodata the function owns (jump tables, string literals, float constants) are migrated with
  it when splat's layout allows; otherwise the gap is written down in the commit.

## GCC/Allegrex specifics to keep in mind

Expectations to confirm per unit, not established facts about this binary:

- **`-G0`** is a fact (FACTS): don't expect `$gp`-relative data; globals are `lui`/`addiu` pairs.
- **EABI32 + `long32`** (FACTS: `e_flags`, `.gcc_compiled_long32`): EABI passes more arguments in
  registers than O32 (`$a0–$a3` plus `$t0–$t3`) — check the m2c target's assumptions against the asm.
- **C++**: vtables live in `.rodata`; constructors are listed in `.ctors` (334 entries) — each one is a
  static initializer for a translation unit, which is evidence for **TU boundaries**. GCC 3.3/4.0 both
  use the Itanium mangling once names are known.
- **VFPU** (0.18 % of instructions, concentrated in `0x300000–0x3fffff`): m2c doesn't know it, objdiff
  rendering is *unverified*, decomp.me's assembler likely can't take it. Expect inline asm or intrinsics
  macros in `include/` for those functions; leave them for after the toolchain is proven.
- **Statically linked SDK code** (libc, libstdc++, SGX) is identified first and either matched as a
  library unit or kept as asm — it is noise in the progress number for the game itself, so it is
  reported separately.

## Progress is reported honestly

- Two numbers, never merged: **game code** and **SDK/library code**, each as matched bytes / total bytes
  of `.text`.
- `NON_MATCHING` functions count as not matched.
- Progress claims in a PR come with the `objdiff-cli report` output that produced them.
