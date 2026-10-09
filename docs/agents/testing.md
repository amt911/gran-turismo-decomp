# Testing — the checksum gate, and tests for the tools

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Two kinds of correctness

| What | Oracle | When it runs |
| --- | --- | --- |
| The decompiled game | **byte identity** with the original segments (`ninja check`, *planned*) and objdiff per function | every commit from Phase 3 on |
| The tooling in `tools/` | **pytest**, TDD, coverage ≥ 80 % | every change to `tools/` |

The decomp has no unit tests and needs none: a function either rebuilds to the same bytes or it
doesn't. Don't write tests that re-assert what the checksum already proves.

## Hard rules

- **Never claim done without showing output** — the `ninja check` line, the objdiff numbers, the
  pytest summary. "It compiles" is not "it matches"; "tests pass" without output is not evidence.
- **The checksum files are inputs, not outputs.** They are written once from the verified dump and
  changed only when the target changes — never to make a build green.
- **A tool bug gets a failing test first**, then the fix (`systematic-debugging`).
- **Never delete, skip or `xfail` a test to get green.** Fix the code or the test on purpose.
- **No copyrighted bytes in fixtures.** Tool tests use synthetic inputs: hand-built ELF/PRX headers,
  tiny fake `~PSP` headers, made-up sections. A test that needs the real dump is marked
  `@pytest.mark.needs_dump` and skips (with a reason) when `extracted/` is absent — it never ships data.

## TDD for `tools/`

For new logic in `tools/` (header parsers, the extraction pipeline, NID resolution, progress reports,
config generators):

1. **Red** — a failing test that describes the behaviour, with expected values written by hand.
2. **Green** — the minimum to pass.
3. **Refactor** under green.

Exceptions: one-off shell glue that only calls other tools in order (its proof is running it and
checking the hashes it prints), and spikes — tests before merging.

Layout *(planned)*: `tools/<module>.py`, tests in `tools/tests/test_<module>.py`, run with
`.venv/bin/pytest tools/tests` inside the memory cgroup when it uses workers.
