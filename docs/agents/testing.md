# Testing — the differential gate, and tests for the tools

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Three kinds of correctness

| What | Oracle | When it runs |
| --- | --- | --- |
| A decompiled function | **differential**: same captured inputs → same return, memory writes and calls as the recompiled original (*planned*) | every commit from Phase 3 on, for the functions it touches; all of them before push |
| The game as a whole | **boot check**: reaches the reference states, compared with the original in PPSSPP ([runtime verification](runtime-verification.md)) | every build change |
| The tooling in `tools/` | **pytest**, TDD, coverage ≥ 80 % | every change to `tools/` |

The recompiled original is the reference because it is a mechanical translation of the real code;
when the recompiled build and PPSSPP disagree, that is a recompiler/host bug — fix or report it
upstream before trusting either for that function.

## Hard rules

- **Never claim done without showing output** — the harness summary (cases run / equal), the boot
  check result, the pytest summary. "It compiles" is not "it behaves the same".
- **Captured cases are inputs, not outputs.** They are recorded from the recompiled original and
  changed only on purpose (a capture bug, a new state to cover), with the reason in the commit —
  never re-recorded from the decompiled version.
- **A tool or harness bug gets a failing test first**, then the fix (`systematic-debugging`).
- **Never delete, skip or `xfail` a test or a case to get green.** Fix the code or the test on purpose.
- **No copyrighted bytes in fixtures.** Tool tests use synthetic inputs: hand-built ELF/PRX headers,
  tiny fake `~PSP` headers, made-up sections. Tests that need the real dump are marked
  `@pytest.mark.needs_dump` and skip (with a reason) when `extracted/` is absent.

## TDD for `tools/` and the harness

For new logic in `tools/` and in the harness (header parsers, the extraction pipeline, NID resolution,
capture/compare, progress reports):

1. **Red** — a failing test that describes the behaviour, with expected values written by hand.
2. **Green** — the minimum to pass.
3. **Refactor** under green.

Exceptions: one-off shell glue that only calls other tools in order (its proof is running it and
checking the hashes it prints), and spikes — tests before merging.

Layout *(planned)*: `tools/<module>.py`, tests in `tools/tests/test_<module>.py`, run with
`.venv/bin/pytest tools/tests` inside the memory cgroup when it uses workers.
