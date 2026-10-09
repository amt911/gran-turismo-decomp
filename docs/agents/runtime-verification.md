# Runtime verification — what the harness can't prove

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

The differential harness proves one function at a time on the inputs it captured. It cannot see
timing, threads interleaving, the host layer, or an input nobody captured. The **boot check** covers
that: the whole game, built from this tree, reaches defined states and looks the same as the original.

## The check

1. Build the game in the variant under test (recompiled baseline + whatever is decompiled).
2. Run it, and run the **original** in PPSSPP from the user's dump (`game_files/` / `extracted/`,
   never copied into the repo).
3. Reach each **reference state** — title screen, a menu, a race start, a few seconds of a race with
   recorded input — and compare: screenshots, and key memory/register values at a breakpoint.
4. Record the command, the states and the result in the PR.

The reference states, their input recordings and how to reach them are defined in the Phase 2 spec
and listed here once they exist. Until then this is the contract.

## Rules

- **A check that has never failed is not proven.** Break the build on purpose once (e.g. change one
  constant in a decompiled function) and watch the check go red before trusting it.
- **Expected values are captured from the original game, by hand, once** — not recomputed from the
  build under test.
- **Emulator and recompiler are both claims about hardware.** PPSSPP is the reference for "what the
  original does" because it runs the real binary; a PPSSPP pass is strong evidence, not proof of
  real-PSP behaviour. Say which one you ran.
- **Determinism first** — fixed seeds, recorded input, no wall-clock dependence in the compared
  window; a flaky state is fixed in the harness, not retried until green.
- **One agent per game instance / emulator / debugger port** — each is an exclusive resource.
