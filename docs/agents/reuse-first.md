# Reuse first — search before you write

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Reuse first — search before you write

In a decomp the default failure mode is the **second declaration of the same thing**: two structs for
one game object with different field names, a function named twice under two addresses' worth of
guesses, a `u32` typedef in three headers. Nothing breaks at first — the match doesn't care about
names — and then two units disagree about a struct's layout and one of them stops matching for a
reason nobody can see.

- **Search before declaring. Every time.** Before adding a struct, typedef, enum, macro, extern or
  symbol name, look for it by name *and* by address/size: `grep -rn` over `include/` and `src/`,
  `config/symbol_addrs.txt`, and [FACTS](../FACTS.md). A field offset you're about to define is often
  already known under another name.
- **One definition per type, in `include/`.** A struct used by two units lives in a shared header;
  a unit-local struct moves there the moment a second unit needs it — not a copy in the second `.c`.
- **One name per address.** `config/symbol_addrs.txt` is the single source for symbol names; headers
  and `src/` follow it. Renaming means changing it there and everywhere in the same commit.
- **Extend, don't fork, the tools.** A new splat segment type, a second ELF parser, a parallel NID
  resolver: check `tools/` first and add the parameter to what exists.
- **Rule of three for macros and helpers in `include/`.** Two occurrences can wait; at the third,
  extract in the same change and delete the copies.
- **Don't merge coincidences.** Two structs with the same size and similar fields that belong to
  different owners (a car's physics state and a replay frame) are not one type — unifying them
  couples two units that never shared code. Reuse what shares *a reason to change*, not a shape.
- **A deliberate duplicate is one sentence in the commit** — say why the shared one didn't fit.

Dependencies count as existing code: before hand-rolling an ELF parser or a MIPS decoder in `tools/`,
check what splat, spimdisasm, rabbitizer or `pyelftools` already give. New dependencies follow
*Working rules* in `AGENTS.md` (install, pin, say so).
