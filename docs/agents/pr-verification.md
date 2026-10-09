# Agentic PR verification (MANDATORY on every PR)

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Agentic PR verification (MANDATORY on every PR)

**Every PR MUST be verified before merge, and the verdict MUST be posted as a PR comment**
(`gh pr comment`). Running the pass and posting the verdict is **not optional**. There is no app to
click through here, so the "drive the running thing end to end" engine is adapted to a decomp: a
headless agent **rebuilds the PR from a clean tree with the user's dump and checks the gate**, then
reads the diff for what the gate can't see, posts the verdict, and **waits for the user to merge**.

### What the pass does

1. **Clean tree** — a fresh git worktree of the PR branch (never the author's working tree), with
   `game_files/` pointing at the user's ISO.
2. **Rebuild from zero** *(commands planned, Phase 1–3)* — `tools/setup.sh`, `tools/extract.sh` (hash
   checks must pass), `ninja`, `ninja check`, all inside the memory cgroup.
3. **Measure** — `objdiff-cli report` on the branch and on `main`; the verdict lists the delta per unit
   (newly matched, regressed, `NON_MATCHING` added) and the two progress numbers (game / SDK).
4. **Read the diff for what the gate can't see**:
   - copyrighted data or generated `asm/` staged anywhere (`git diff --stat`, `git status --ignored`);
   - edits to checksums, `expected/` or generated asm;
   - names without evidence in `config/symbol_addrs.txt`;
   - a `NON_MATCHING` block without its note, or one that replaced the asm in the default build;
   - duplicated struct/typedef declarations ([reuse first](reuse-first.md));
   - FACTS/FINDINGS/ROADMAP not updated for what the PR changed.
5. **Off-gate builds** (shiftable / non-matching / mod) — the PPSSPP check from
   [runtime verification](runtime-verification.md), with the state reached.
6. **Post** the verdict with `gh pr comment`: commands run, their output (hashes, numbers), findings.

### Limits

- **Advisory, never a veto on its own** — the deterministic gate (`ninja check`) is the hard one; the
  agent's reading of the diff is findings, not a block. A finding is not a reproduction (see
  [debugging](debugging.md#a-finding-is-not-a-reproduction)).
- **Never merges**, in any mode — see `AGENTS.md` § Git & GitHub.
- **Trigger:** in "modo desatendido" the agent opens the PR and runs the pass itself; in "normal mode"
  the user opens the PR and the agent runs the pass before merge. Scope the headless run's allowed
  tools to what it needs; confirm flag names with `claude -p --help` rather than from memory.
