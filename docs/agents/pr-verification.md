# Agentic PR verification (MANDATORY on every PR)

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Agentic PR verification (MANDATORY on every PR)

**Every PR MUST be verified before merge, and the verdict MUST be posted as a PR comment**
(`gh pr comment`). Running the pass and posting the verdict is **not optional**. The "drive the
running thing end to end" engine is adapted to a decomp: a headless agent **rebuilds the PR from a
clean tree with the user's dump, runs the differential gate and the boot check**, reads the diff for
what neither can see, posts the verdict, and **waits for the user to merge**.

### What the pass does

1. **Clean tree** — a fresh git worktree of the PR branch (never the author's working tree), with
   `game_files/` pointing at the user's ISO.
2. **Rebuild from zero** — setup, extract (hash checks must pass), recomp, build, all inside the memory
   cgroup. `tools/verdict.sh <branch> <tag>` runs steps 2-3 on `../gt-verify` (`GT_VERIFY_TREE`): pytest,
   ctest, the full gate under `taskset -c 6-11,18-23`, the `save_garage` boot check and the synthetic gate after
   `dealer,race` and `save_garage,circuit,buy,options`, one group after the other (synthetic runs are CPU-bound: six at once all
   timed out, 2026-11-10). The synthetic groups take their own lock (`CPU_LOCK_DIR=.../synth`) and cores
   0-5,12-17, so one verdict's synthetic gate runs while another's real-time gate routes (cores 6-11,18-23) run. A route that exits 97 is
   re-run alone before the verdict is written. **The synthetic gate is skipped** when no changed path can reach it
   (`tools/verdict_scope.py`: an allowlist of docs, mods' checks, pad routes, the mod host and renderer-only patches;
   anything else, `CMakeLists.txt` included, runs it) — user decision 2026-11-10. The build is incremental in the
   verify tree (the generated code is only rewritten when it changes), so a verdict's time is the two gates.
3. **Run the gates** — the full differential suite and the boot check
   ([runtime verification](runtime-verification.md)); the verdict lists per function: newly
   decompiled, regressed, cases run, and the two progress numbers (game / SDK) against `main`.
4. **Read the diff for what the gates can't see**:
   - copyrighted data or generated `recomp/` code staged anywhere (`git diff --stat`,
     `git status --ignored`), or generated C++ pasted into `src/`;
   - captured cases deleted, edited or re-recorded from the decompiled version;
   - names without evidence in `config/symbol_addrs.txt`;
   - decompiled code assuming host pointers before the interop spec allows it;
   - third-party code without its licence notice;
   - duplicated struct/typedef declarations ([reuse first](reuse-first.md));
   - FACTS/FINDINGS/ROADMAP not updated for what the PR changed.
5. **Post** the verdict with `gh pr comment`: commands run, their output (hashes, case counts,
   states reached), findings.

### Limits

- **Advisory, never a veto on its own** — the deterministic gates (differential suite, boot check)
  are the hard ones; the agent's reading of the diff is findings, not a block. A finding is not a
  reproduction (see [debugging](debugging.md#a-finding-is-not-a-reproduction)).
- **Never merges**, in any mode — see `AGENTS.md` § Git & GitHub.
- **Trigger:** in "modo desatendido" the agent opens the PR and runs the pass itself; in "normal mode"
  the user opens the PR and the agent runs the pass before merge. Scope the headless run's allowed
  tools to what it needs; confirm flag names with `claude -p --help` rather than from memory.
