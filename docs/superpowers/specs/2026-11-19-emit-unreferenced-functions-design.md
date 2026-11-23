# Emit the unreferenced functions — design (2026-11-19)

Status: **approved by the user on 2026-11-19**. (design only, no code). Design calls of the author are marked "(Claude)".

## Problem

`tools.progress` counts 23,208 functions (Ghidra's list, `recomp/analysis/functions.tsv`). A decompiled function can only be registered
(and gated) if the recompiler emitted its original: `[decomp] … no recompiled function at …, not installed` otherwise. Of the 23,208:

- 306 lie outside `.text` (the Sony import stubs, 2 instructions each; `import_wrappers: 329` in `auto_codegen_report.json`). They are
  never emitted as game code and never registrable: out of scope here.
- 22,902 lie inside `.text`; **2,368 of them are never emitted** (no `runtime.register_function(…)` in `recomp/generated/`). So the best
  reachable ceiling today is 22,902 − 2,368 = 20,534 functions registrable (88.5 % of 23,208), while 2,368 (10.2 %) can never move the number.

## Why they are not emitted (measured 2026-11-19, also recorded in `docs/FINDINGS.md`)

`psp_recomp --auto` is a global CFG discovery: it seeds from the ELF entry, `j`/`jal` targets found in reached code, relocated data words that
hold code addresses and `lui`/`addiu` pairs (patch 0008), then emits each reached instruction once (41,467 seeds, 258,051 block entries,
884,604 instructions, 156 units of 0x8000 bytes). Classification of the 2,368 by a static scan (`jal`/`j` targets, `lui`+memory-op/`addiu` pairs within
10 instructions, raw 32-bit words anywhere in the ELF):

| class | functions |
| --- | ---: |
| no reference of any kind: dead code the linker kept (unused template instances, destructors, copies) | 1,998 |
| a raw data word equals the address (vtable / table slot) but is not seeded (no relocation for that word, or a coincidental value) | 246 |
| called only from other unemitted code | 80 |
| called from an emitted function, from a block the CFG never reached (after a noreturn call, a dead path) | 34 |
| address built by `lui`/`addiu` only in such an unreached block | 10 |
| padding or data that does not decode | 0 |

867 are under 20 instructions, 960 are 20-39, the rest larger; all decode as valid Allegrex. Together 82,372 instructions (+9.3 % over the 884,604 emitted).

## Design

### 1. How the seeds get in (Claude)

A new patch `00NN-codegen-extra-seeds.patch` next to 0014, **a separate seeds file**, not a reuse of the boundaries file: 0014's file lists
addresses the *host replaces* (they are poisoned like imports and every `J/JAL` reaches them through the dispatcher); a seed must do the
opposite (be emitted as ordinary code). Mixing them would make every call into a seeded function go through the dispatcher and change
timing and the "calls 0" behaviour of the gates. `tools.recomp` gains `--extra-seeds FILE` (default `recomp/extra_seeds.txt`, generated, ignored by git, like
`decomp_boundaries.txt`), one hex ELF-runtime address per line, `#` comments. `program_analysis.cpp::add_seed` gets a new source tag `extra_seed`
and the seed is added exactly like `direct_jal_target`: the CFG walk then covers the function.
The file is produced by a new tool `tools/extra_seeds.py` (TDD, pytest ≥ 80 %) from `functions.tsv` minus the registrations of the generated units,
filtered by the evidence rules in § 4. No game bytes are stored in git: the list is derived locally, and only the tool and its tests are committed.

### 2. What changes in the generated baseline, and why emitted behaviour does not

Emission is "each reached instruction once"; a seed only **adds** reached instructions. Therefore:
- every already-emitted instruction, block entry and function entry stays emitted with identical C++ for its own block; what can change is the
  *unit it lands in* only if the unit partition (fixed span 0x8000) changes — it does not, the partition is by address;
- units that receive new blocks are rewritten (their `entry_labels` tables grow, a dense dispatch table may become sparse or vice versa: codegen's
  `dense_dispatch` choice depends on the label span of the unit). That is a **code-shape change of existing functions' units**, so it is not assumed
  harmless; it is proven (below);
- a new seed whose code *falls into or jumps into* an existing block (a function that tail-shares a block with an emitted function) creates a new
  entry label inside an existing block: single-copy emission splits the block there. Existing control flow is unchanged (the fallthrough becomes an
  explicit label), again proven below.

Function boundaries of existing functions do not move: they come from Ghidra's `functions.tsv`, which this work does not touch (it is an input,
not an output, of the recompiler), and emission by address does not depend on them. **Check:** `tools/extra_seeds.py --verify` asserts (a) the set of
`register_function` addresses after is a superset of the set before, (b) every instruction address emitted before is emitted after, (c) `functions.tsv` is
byte-identical.

Proof of unchanged behaviour (all three before merging, results kept in the PR):
1. **Generated-set diff** (above): superset, no removed address; diff of the C++ per unit reported (only units with new seeds differ).
2. **Trace**: `build/gt_native --max-seconds 30 --trace` before and after: identical trace (`tools.trace_compare` exit 0), and the boot check reaches the reference states.
3. **Full gate before and after** on the same commit of `src/`: same 11 routes, compared totals within the gate's own floor rule, 0 mismatches both times,
   `config/decomp_gate_floor.txt` not lowered (the floor then ratchets up only through a normal PR).
4. **Synthetic gate**: the existing verified registrations (`tools.decomp_gate synth` over all of them) give the same compared/equal counts before and after.
A difference in any of the four blocks the PR; the seeds list is bisected by halves to find the offending seed.

### 3. How the new functions are verified

They are dead code (that is why they were not reached): no route will ever call them, so route cases are impossible. They are verified **only by synthetic
cases** (`tools.decomp_gate synth`, `sabotage --synth`), counted apart as "verified by synthetic cases", exactly as the existing singletons
(decomp workflow, § "Functions no route reaches"). Some will stay "no comparable synthetic case" (originals that fault on arena pointers); the corpus search
(`tools.decomp_gate coverage --synth --search`) is the second tool. The README counts the two kinds apart and this work never claims more.
The 246 data-referenced ones (vtable slots) additionally get recorded-argument cases when a route reaches their vtable (they are reachable in principle).

### 4. The Ghidra wrong-boundary risk, and how it is detected (Claude)

Some entries of `functions.tsv` start mid-function (example: `0x08BF9F3C` begins with `bne $a2,$a3,-> 0x08BF9ED4`, a branch back into the preceding function, and
ends with a different function's prologue after its `jr $ra`). Seeding it makes the recompiler emit a second entry into code that is not a function. The seed filter
therefore demands **evidence that the address is a real function start**, any one of:
- the address is the target of a `jal`/`j` anywhere in `.text` (including unemitted code) or of a relocated data word (the 80 + 34 + 246 classes carry this evidence by construction);
- it begins with a stack-frame prologue (`addiu $sp,$sp,-N` possibly after one or two instructions that do not use `$ra`/branch) **and** no branch of the preceding function (by Ghidra's own boundaries) targets an address inside it;
- it follows a `jr $ra` + delay slot (and alignment nops) of the previous function.
Rejected (not seeded, listed in the tool's report): an entry whose first instructions branch to an address outside `[start, start+size)` backwards, an entry that is reached
by a branch from the previous function (shared tail), and an entry with no evidence and no prologue. The tool prints the rejected list with the reason; the rollout phase for the 1,998 shows the count
rejected before any seed is emitted. A rejected function stays "not emitted" and the ceiling below is reduced by it. A seed that nonetheless decodes into garbage is caught by the recompiler
(`unsupported` / `invalid internal function entry` in the generated code) and by the before/after proof.

### 5. Rollout (Claude)

One PR per phase, never stacked, each with the four proofs of § 2:
1. **Phase A — referenced ones:** the 246 data-referenced + 80 called-from-unemitted + 34 called-from-unreached-block + 10 address-in-unreached-block (370 total; the coordinator's "~124 call-referenced" = 80 + 34 + 10). They have reference evidence, so the boundary risk is lowest.
2. **Phase B — the 1,998 dead ones, filtered by § 4**, in slices of about 500 by address so a failing slice is bisected cheaply.
3. Decompiling the newly emitted functions follows the normal batch flow (15-25 per PR, synthetic gate), and the README gets a second line "of which verified by synthetic cases only".

### 6. Expected ceiling

Registrable functions: 22,902 − rejected (§ 4). With all of them emitted the ceiling is 22,902 / 23,208 = **98.7 %** (the 306 import stubs can never count). Realistically
the percentage reached through this work is bounded by the synthetic gate's reach (it compared 26 of 36 drafts of the first new batch: ~70 %), so the dead functions are expected to land at
roughly 60-75 % of 2,368 verified, i.e. **about +1,400 to +1,800 functions = +6 to +8 points** on top of whatever the normal pending work gives.
These numbers are estimates from one batch; the first slice of phase B measures the real rate before the rest is committed to.

### 7. Cost (estimates, to be measured in phase A)

- Generated C++: +82,372 instructions (+9.3 %) in the 156 units; roughly +9 % `.cpp` size; `libgt_generated.a` (118 MB) about +10 MB; `gt_native` (92 MB) about +9 MB.
- `tools.recomp`: ~4 min / 3.8 GB today (exclusive heavy slot); expect +10 %. A seed list change rewrites only the units that gain blocks (9 of 156 rewritten in a typical 20-registration change; a seed slice touches the units its addresses live in, i.e. many: expect most units for phase B).
- Build: a seed PR rebuilds the touched generated units (measured 150-290 s per unit at load 39, `-j4`): phase B ≈ a full rebuild of the generated library once (≈ 1.5-3 h on a loaded machine, 20-30 min idle).
- Gate time: the real-time routes do not execute the new code, so route time is unchanged; the synthetic gate gains one forked child per registered new function (cases ≈ 64 each), a few seconds per function.
- Risk to existing speed: the generated dispatch tables grow; measured in the trace check (§ 2.2) and the frame-pacing route, not assumed.

## Not in this design

Changing Ghidra's function list; treating import stubs as functions; emitting code that no evidence says is code; any change to what the already-registered decompiled functions do.

## Decisions for the user

1. Approve the separate seeds file (§ 1) rather than extending the 0014 boundaries file.
2. Approve phases A then B, and the evidence filter of § 4 (a seed needs reference evidence or a prologue not entered from the previous function).
3. Approve that these functions are reported apart as "verified by synthetic cases only" and that dead code is not claimed as route-verified.
