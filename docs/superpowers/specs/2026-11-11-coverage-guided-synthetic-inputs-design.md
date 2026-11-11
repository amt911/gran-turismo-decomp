# Coverage-guided synthetic inputs (option E) — design

Status: **approved by the user 2026-11-11; implementation follows in a separate PR.** Follows `2026-11-10-recorded-arguments-design.md` (option B merged; C and D
not built) and the depth measurement in FINDINGS ("How deep synthetic cases go").

## Problem

Synthetic cases compare a decompiled function with its recompiled original on generated inputs. Measured on 120 decompiled
functions of 300-1000 B that no route reaches: 67 are "verified" (>= 1 equal case, no mismatch) but the cases take a **mean of
30 % of each function's branch outcomes** (median 25 %; 1 of 67 reaches 75 %, 4 take every branch). The decomp workflow wants the
paths real play reaches to be covered, so a synthetic "verified" is weaker than it looks, and the unexercised branches are exactly
where decompilation mistakes hide (the class-matched `a0` of option B found four latent bugs by reaching a few more).

## Idea

Steer the generated inputs by what they execute. Per function, run a bounded search: generate a case, run it through the existing
harness, keep it when it executes a branch no earlier case did, and mutate kept cases to reach more. Keep the winners as a small
**corpus** of deterministic recipes; the gate replays the corpus as ordinary synthetic cases (same comparison, no coverage needed).

## Which code's coverage guides the search

Not the recompiled original: the generated C++ has no counters, and instrumenting 258k generated functions is out of
proportion. Instead the **decompiled function's own source coverage** (clang `-fprofile-instr-generate`, the build
`tools.decomp_gate coverage --synth` already makes for the files that register the chosen functions). Branch outcomes the decompiled
code takes are, by the gate's equality, the ones the original takes on that case; a branch the cases never take in the decompiled
code is a path nothing compared. Consequence: the search runs only in a coverage build of the files under test, never in the gate.

## Design

1. **Coverage signal per case.** In the case child (one per function, already forked), after each case read the instrumentation
   counters (`__llvm_profile_begin_counters` / `end`, weak symbols like the profile dump in `host/diff_harness.cpp`) and compare
   the number of non-zero counters with the previous total. Counters are cumulative inside the child, so "more non-zero counters"
   = the case reached something new. No per-function counter map is needed because a child runs one function (callees it
   reaches in other instrumented files count too, which is fine: they are paths of that function's cases).
2. **Search loop (`GT_SYNTH_SEARCH=<cases per function>`, default off).** Seed = today's `make_synth_case(function, index, seed)`
   cases 0..K-1. A case that increases coverage joins the corpus. Further cases are `mutate(parent, rng)`: change one argument
   register (to an arena pointer, a live object, a class object, a small int, -1, 0, a float edge value), flip or replace one
   arena word, or replace one 1 KiB arena block with a fresh draw. Budget per function: N cases or T seconds (default 200 cases /
   10 s), whichever first; stop early after M cases without gain (default 40).
3. **Recipe, not data.** A corpus entry is `ADDRESS INDEX SEED MUT...` where each mutation is `reg:R=VALUE`, `word:OFFSET=VALUE`
   or `block:INDEX=SEED`. It contains no game bytes (values are pointers into the synthetic arena or small constants) and
   regenerates the exact inputs with the existing generator plus the mutation list. Stored as `config/synth_corpus/<ADDRESS>.txt`
   (text, a few lines per function, tracked in git; a function with no gain has no file).
4. **Replay.** `make_synth_case` takes an optional recipe; `run_synthetic_cases` runs the corpus entries of each installed function
   after its normal cases (`GT_SYNTH_CORPUS=config/synth_corpus`, on by default once the directory exists). The comparison, sabotage
   and report are unchanged; the report prints how many corpus cases were equal / mismatched.
5. **Tool.** `tools.synth_search ADDR... [--budget ...]` builds the coverage binary for the files that register the functions
   (reusing `gate_coverage.source_files`), runs the search run, and writes the corpus files; `tools.synth_corpus` (pytest) parses,
   validates and merges recipes. `tools.decomp_gate coverage --synth` reports the depth again with the corpus, so the effect is
   measured with the same instrument.
6. **Mismatches.** A corpus case that mismatches is a finding (a decompiled-function bug), listed in FINDINGS like the others; it is
   not dropped from the corpus (the gate rule: never weaken the gate).

## Success measure (before the corpus is trusted)

On the same 120-function sample (dealer scene, seed 1): mean branch-outcome coverage of the verified functions from 30 % to >= 60 %,
at <= 10 s per function of search; the corpus replay adds <= 2 s per 100 functions to the synthetic gate. If the first search stalls
under 45 %, the report says which branches never flipped (typically ones guarded by tables or callee results the arena cannot
produce) and the design stops there rather than growing.

## Testing

- C++ (`host/tests/synth_case_test.cpp`): mutation operators are deterministic, stay inside the arena, and a recipe replays to
  the same inputs; corpus parsing rejects malformed lines.
- pytest (`tools/tests`, coverage >= 80 %): recipe parser/merger, corpus file writer, search command line.
- Sabotage: flip a branch of a decompiled function that only a corpus case takes; `sabotage --synth` with the corpus must pin it and
  without the corpus must not (that pair is the proof that the corpus adds verification).
- Full gate and boot check as always; the corpus changes nothing for route gates.

## Costs and risks

- The search needs a coverage build and one run per batch of functions (minutes), not per PR gate.
- Counter reading assumes the instrumented files are linked into `gt_native` (true for `GT_COVERAGE_FILES`).
- A function whose original faults on most inputs (53 of 120 in the sample) has nothing to steer by: the search can only help the
  comparable ones; raising that share is option B's class-matched `a0` and any future record-based work, not this.
- Corpus drift: a corpus recorded against one scene (dealer) replays against another route's scene; entries that stop being
  comparable are skipped like any case. Recipes are tied to the arena layout (`arena_bytes`, block size); a layout change
  invalidates them (the file header stores the layout version).

## Not in scope

Options C (donor-triggered cases) and D (record/replay); instrumenting the recompiled original; changing what "done" means.
