# Runtime verification — what the checksum can't prove

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

While the build is byte-identical, booting it proves nothing the checksum hasn't — skip it. Runtime
verification becomes **mandatory** the moment a build is allowed to differ from the original:

- a **shiftable** build (code moved, sizes changed),
- a **NON_MATCHING** build (`-DNON_MATCHING`) used to test equivalence,
- any **mod / port** track built on the decomp.

## The check

1. Rebuild the executable in the variant under test, and **re-wrap** it in a form PPSSPP loads
   (a decrypted/plain PRX or ELF in place of `EBOOT.BIN` in an extracted tree — exact procedure
   *unverified*, to be written when Phase 5 starts).
2. Boot in PPSSPP (headless with `--timeout`, or GUI with the remote debugger) from the user's dump —
   files under `game_files/` / `extracted/`, never copied into the repo.
3. Reach a **defined state** (title screen; a menu; a race start) and compare with the original:
   screenshot compare (`--screenshot-save` / `--compare` in headless), or memory/register state at a
   breakpoint via the websocket debugger.
4. Record the command, the state and the result in the PR.

## Rules

- **A check that has never failed is not proven.** Break the build on purpose once (e.g. patch one
  constant) and watch the check go red before trusting it.
- **Expected values are captured from the original game, by hand, once** — not recomputed from the
  build under test.
- **The emulator is a claim about hardware.** A PPSSPP pass is strong evidence, not proof of real-PSP
  behaviour; say which one you ran.
- **One agent per PPSSPP instance / debugger port** — it is an exclusive resource.
