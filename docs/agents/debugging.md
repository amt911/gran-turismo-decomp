# Debugging — keep the loop from running away

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Debugging — keep the loop from running away

What a bug costs is not the fix. It is the number of times you go around
`build → deploy → reach the state → observe` before you know what to fix, multiplied by what one
lap costs. Everything below attacks one of those two factors. **Each rule carries the number it
came from** — a real 15 h session — because a rule with no measured cost behind it gets deleted in
the first cleanup. Where this project hasn't measured its own, the number is marked
`<!-- pendiente de medir -->` until someone does.

### The loop is the cost

- **Measure before you ablate.** Ablation costs one lap per hypothesis and answers yes/no.
  Instrumentation costs one lap total and answers *what is actually happening*. **Measured: 28
  ablations over 1 h 42 min ruled things out and moved nothing; a single batch of probes, 13 min,
  changed the question and the bug fell on the next round.** The rule that batch produced: **if a
  pipeline completes every phase with non-empty output, the output exists** — stop asking "why
  doesn't it appear" and start asking "where does it appear". They are different questions and the
  second one is cheap.
  **Here the pipeline is** `split (splat) → compile (one unit) → assemble/link → compare (objdiff /
  checksum) → (only off-gate) boot in PPSSPP`. For a stubborn mismatch, probe the stages instead of
  guessing: diff the unit's `.o` against the expected one *before* linking, dump the relocations of
  both, check the section the symbol landed in. A mismatch that appears only after linking is a
  layout/split problem, not a C problem.
- **Budget the lap, then attack the dominant term.** Time the four phases once and write the real
  numbers into the table below; one of them dominates and the other three are noise. In the measured
  case "reach the state" was 60 s × 30 reproductions — half an hour of pure waiting — and it died to
  a shortcut nobody had bothered to write. **If a bug needs more than three reproductions, write the
  shortcut before the fourth**: a deep link, a dev-only route, an environment snapshot, a seeded
  fixture. Commit it as `tools/repro-<bug>.sh` and name it in the `docs/FINDINGS.md` entry, so
  the next person pays zero.

  | Lap phase | Command here | Measured |
  | --- | --- | --- |
  | split (only when the yaml changed) | `splat split config/<target>.yaml` *(planned)* | <!-- pendiente de medir --> |
  | build one unit | `ninja build/src/<unit>.o` *(planned)* | <!-- pendiente de medir --> |
  | compare | `objdiff-cli diff -u <unit> <function>` *(planned)* | <!-- pendiente de medir --> |
  | runtime (off-gate builds only) | PPSSPP boot to the state under test | <!-- pendiente de medir --> |

  The usual shortcut here is not a deep link but a **narrower compare**: one unit, one function,
  no full link — and decomp.me for a fast compiler round-trip once the compiler is known.

### A finding is not a reproduction

- **Whoever reviewed read the code; they did not run it.** Reproduce a review finding yourself
  before sending anyone to fix it, and **if the implementer says they can't reproduce it, believe
  the implementer over the reviewer** — one of them has the thing running. **Measured: 1 h 25 min
  spent chasing a bug that did not exist.** This is the same reason the agentic PR pass is advisory
  and never vetoes on its own (see
  [Agentic PR verification](pr-verification.md#agentic-pr-verification-mandatory-on-every-pr)), and the reason
  `receiving-code-review` asks for verification rather than agreement.
- **A test that refuses to go red is data, not a failure.** The fourth failed attempt to pin down
  that non-existent bug is precisely what uncovered the real one, pointing the opposite way.
  Reporting "I cannot make this fail" is a result and it gets reported; covering it with a green
  test throws away the only signal the round produced.
- **Before demanding a red, ask whether the mechanism can produce one.** If another layer of the
  framework masks the effect, no amount of insisting will turn the test red, and the time goes into
  the test instead of into the bug. **Measured: over 1 h on two reds that were structurally
  impossible.** Establish that the failure is observable at that layer first; if it isn't, move the
  assertion to the layer where it is — that is what
  [Runtime verification](runtime-verification.md#runtime-verification--what-the-checksum-cant-prove)
  is for.

### Tests that cannot fail

The template's mutation gate (deferred here — see `AGENTS.md` § Rules by topic) names the worst case —
an expectation recomputed with the same expression the code uses, which moves with the mutation and
agrees with it. It is not the only one. **Enumerate for this stack the assertions that are inert by
construction**, because none of them show up as a failure, a warning, or a coverage drop:

| Inert by | Looks like | Applies here |
| --- | --- | --- |
| assertions disabled at runtime | the assert keyword compiled out or off by default | `python -O` strips plain `assert` in `tools/` code (pytest only rewrites asserts in test files): tools validate input with `raise`, never `assert` |
| self-writing snapshots | first run records whatever happened and calls it expected | **`expected/` objects or `*.sha1` regenerated from the build under test** — everything "matches". Expected comes only from the original dump / the asm-only build of it |
| permissive mocks | the double returns the expected value by default, unasked | `unittest.mock.MagicMock` answers any attribute with a truthy mock: use `create_autospec` / `spec=` |
| expectation computed like the code | same expression on both sides of the equals | a parser test that builds its expected offsets with the parser's own `struct` format: write the bytes and values by hand |

**Every assertion is watched failing once**, and expected values are written out by hand. This is
the same rule [Runtime verification](runtime-verification.md#runtime-verification--what-the-checksum-cant-prove)
states for on-device checks — it applies to in-process tests with no exception.

### The environment is a claim until it is measured

- **Verify the limit reaches the process doing the work** — see the check in
  [Heavy jobs run inside a memory cgroup](../../AGENTS.md#-heavy-jobs-run-inside-a-memory-cgroup-mandatory). A
  wrapper that reports success over an idle process is worse than no wrapper: it buys confidence
  and delivers nothing.
- **Environment claims get measured or they don't get made.** "That heap sounds low" produced a
  recommendation that was simply wrong. Measuring it — three runs per setting, GC pause totals, real
  peaks, not one run each — gave a **0.4% difference, below the run-to-run variance**. **No
  performance tuning lands without a before/after over more than one run**, and a difference smaller
  than the spread between runs is not a difference.

### Locate the rule before you pick a side

- **A rule that lives in one layer and isn't shared by the others fails in the wrong place.** The
  symptom surfaces where the assumption breaks, not where it is written, which is why the fix keeps
  landing in the innocent layer. Find which layer owns the rule first, then decide which side gives.
  The **contract test** row in
  [Runtime verification](runtime-verification.md) is how you pin one
  down once you know it exists.
- **Replacing a component can remove capabilities in silence.** When you swap one API for another,
  enumerate what the old one did that the new one does not, and say it out loud in the PR — nothing
  will fail to compile. **An optional parameter that defaults to off is a capability that only
  exists if the caller remembers it**, which over a few months means it does not exist.
