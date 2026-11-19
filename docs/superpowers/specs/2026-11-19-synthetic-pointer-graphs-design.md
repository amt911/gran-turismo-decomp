# Synthetic pointer graphs: cases whose dereferenced words are valid memory

Status: **approved by the user on 2026-11-19**. Extends
`2026-10-25-synthetic-cases-design.md`, `2026-11-10-recorded-arguments-design.md` (option B, class-matched a0) and
`2026-11-11-coverage-guided-synthetic-inputs-design.md` (corpus). No code before this is approved.

## Problem (measured 2026-11-19)

136 emitter-body functions of `hot_batch_320`-`331` and `335` (not owned by an open PR, registrations uncommented) were run through
the synthetic gate in the six standard scenes (`dealer,race,save_garage,circuit,buy,options`):

| Cause (first "not comparable" reason of each function) | Functions |
| --- | --- |
| Original faults: guest memory outside RAM at 0x0-0x14 (null holders, small ints used as pointers) only | 67 |
| Original faults at garbage addresses (random arena words, `0xD3FF4A20`-like) only | 35 |
| Original faults at both kinds | 7 |
| "no recompiled function at ADDR, not installed": the registration names an address the current recomp has no function for (16 of them are the "compared equal, sabotage undetected" rows) | 16 |
| "original: no function at 0x...": a case reached a call target that is not a function (a random word used as a callee) | 4 |
| Original reaches a non-recordable import (StdioForUser x2, SysMem, WaitThreadEnd, sceLibFont, IoGetstat, sceNetAdhoc, sceMp3) or switches thread | 9 |
| Compared | 1 |

(The 109 faulting functions are the first three rows; they are the target. The "no recompiled function" 16 are a registration problem, not a generator
one; the 4 "no function" are the same disease as the garbage faults: a word the generator drew was dereferenced or called.) Other scenes
(garage, drift, challenge, dirt, manual, challenge2, trialmtn) compared 8 more, which shows scene state explains part of the tiny-address group, but
not most of it.

Why the generator cannot reach them: `host/synth_case.cpp` fills the 64 KiB arena with words that are 40 % arena pointers, 25 % ints below 17, 10 %
zero, 5 % -1, 10 % floats and 10 % random words. An access chain `a0 -> word -> word -> word` lands on a non-pointer with probability
1 - 0.4^depth; a function that walks a structure two or three levels deep faults in almost every case. The corpus search cannot help: it steers by
coverage of cases that compare, and these never do.

## Proposal

1. **Generator v2 (appended, never replacing).** `make_synth_case` keeps its draws and layout bit for bit. A new `generator_version = 2` adds cases
   with index >= `SYNTH_CASES` (8, i.e. cases 8..15 per function by default, `GT_SYNTH_V2=<n>` sets n, 0 = off) drawn from a separate random
   stream seeded `seed ^ 0x56324B..` so v1 inputs and every existing corpus recipe (`ADDRESS INDEX SEED MUT...`, index < 8 or a stored index) replay
   identically. (Claude: appended cases, not a changed distribution, because the gate must never change an existing comparison.)
2. **Valid RAM for every dereferenced word (pointer closure).** In a v2 case each arena word is drawn from "pointer-shaped" classes only: 70 % a pointer
   to the start of a 16-byte-aligned slot of the arena (so a second and third dereference stays inside), 10 % a pointer to a table block, 10 % zero,
   10 % a small int or float. Words a function reads as a count or length are the risk (a huge loop): small ints stay small (<= 16) and the
   per-case step cap of the harness stays as is. No word is drawn outside RAM or into the code range.
3. **Class-matched a0 and struct shapes.** Where a function's class is known (`class_instances`, vtable runs found by `find_vtable_runs`), a0 is one
   of the game's live objects, as today. v2 adds *struct shapes*: for a function whose address is listed in `config/synth_shapes.txt`
   (`ADDRESS reg:a0=struct:SIZE field:+0x10=ptr field:+0x14=int:0..8 ...`, derived from the asm or from `include/` structs and FACTS; no game bytes)
   the arena slot a0 points to is laid out accordingly. The file is optional and starts empty; the first entries come from the 109 faulting functions'
   first-fault offsets (the harness already logs the faulting address: offset from a0 tells which field must be a pointer). (Claude: a shapes file
   instead of heuristics, because the evidence of each shape stays written next to it, as for names.)
4. **Null holders.** A tiny-address fault at 0x4/0x14 with a0 a live object means a game-state holder is null in the scene; v2 does not invent
   game state. Those functions stay on the scene routes (`GT_SYNTH_ROUTE=...`), and the report prints them as "needs a scene".
5. **Report and corpus.** The run report prints v2 cases apart ("v2 compared N, equal N") so the verified count can say which generator made it.
   Corpus recipes record the generator version in their header; v2 recipes are only written once v2 cases exist.
6. **Registration hygiene (separate, small).** The 16 "no recompiled function" registrations are checked against the recomp's function table by a
   test in `tools/tests` (an `HOT*` registration whose address is not a function start fails the unit test), so they cannot sit silently.

## Existing cases stay stable (required)

- v1 draw order, seeds, arena layout and `scene_pokes` are untouched; a test in `host/tests/synth_case_test.cpp` hashes the inputs of 200 v1 cases
  of 20 addresses before and after the change and requires identical results.
- v2 cases only add comparisons; a function verified today stays verified by the same cases. Nothing is removed, widened or re-ordered.
- Gate time grows by the v2 cases only; with `GT_SYNTH_V2=0` the run is byte for byte today's.

## Measuring success

- Before/after table per source file (`hot_batch_320`...`335`): functions installed, compared >= 1 (v1) and compared >= 1 (v2), on the six standard
  scenes. Baseline above: 1 of 136. Target for the first version: at least 25 of the 109 faulting functions newly compared (that is 23 %, the share
  of the 67 + 35 that fault deep in structures rather than on scene state); if under 10 the design stops and the report says which first-fault offsets
  remain, rather than growing.
- Of the newly compared, those with 0 mismatches and sabotage pinned count as verified by synthetic cases (counted apart as today).
- Sabotage proving the new cases bite: pick a newly compared function whose v1 cases never compared; `sabotage --synth` with `GT_SYNTH_V2` on
  must pin it, with `GT_SYNTH_V2=0` it must report "never compared" (the same pair as the corpus spec). A second check flips one branch only a deep
  dereference reaches and requires detection.
- Mismatches in v2 cases are findings (decompiled-function bugs or a generator shape error); never dropped.

## Cost

- Per function: 8 more cases (default), each a forked child as today; measured v1 cost is ~35 minutes for 31 functions in 6 routes under load (most of
  it route setup and CPU-lock waits), so v2 adds a few minutes. The generator draws 16 K words per case; negligible.
- Memory: unchanged (same arena size). Tool code (`tools/*.py`: report, shapes parser) is TDD with pytest, coverage >= 80 %; C++ tests in
  `host/tests/synth_case_test.cpp`.

## Out of scope

Game-state holders (stay on scene routes), record/replay of real arguments (option D), instrumenting the recompiled original, changing what "verified" means.
