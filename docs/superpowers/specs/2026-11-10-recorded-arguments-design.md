# Recorded arguments for synthetic cases — design (DRAFT, awaiting approval)

Status: **draft 2026-11-10, not approved. No implementation exists beyond the measurement spike (below).**

## Problem as stated

6,359 functions are left to decompile. Routes reach 530 of them; the other 5,829 can only be verified by synthetic
cases, and the workers report 1–8 verifiable functions per PR. Idea to evaluate: record the real arguments (and the
memory they point to) at calls near the target during routes and replay them as synthetic cases.

## Measurements (2026-11-10, main at ddfb9dc; all numbers reproducible with `tools/spikes/recorded_args/`)

Setup: 12 route probe logs (`probe_pbase_*`, every function probed) give the reached set R; `GT_LIVE_SCAN` (spike) scans
the heap every 150 vblanks on race, dealer, garage, options, circuit and drift; full synthetic gate (dealer scene, 8 cases
per function, seed 1) over all 16,853 installed functions, with and without a class-matched `a0` (spike
`GT_SYNTH_CLASS`).

**1. How many of the 5,829 unreached functions could get a recorded argument at all** (static + live):

| Source of a realistic argument for target F | functions |
| --- | --- |
| F in a vtable run (>= 2 consecutive function addresses in data) | 438 |
| ... and some member of that run is reached by a route (a recordable sibling) | 185 |
| ... and a live heap instance of F's class exists at any scan, any of 6 routes | **68** |
| F has a static caller (`jal`) that a route reaches (donor = the caller's frame) | 472 |
| F's address is taken (`lui`/`addiu`) by a reached function | 94 |
| union of sibling, reached caller, taken-by-reached | **749 (12.8 %)** |
| plus callers of callers | 910 (15.6 %) |
| **no `jal` caller, no data pointer, no address taken anywhere** | **2,818 (48 %)** |

Calibration of that last row: among the 6,812 functions routes do reach, only 519 (7.6 %) show no static reference. So
about half of what is left is orphaned (dead code, or reached only through mechanisms no route plays): no recorded
argument can exist for it, because nothing calls it.

**2. How much does argument realism matter?** Calibration on the *already decompiled* functions no route reaches
(10,570), which the synthetic gate can compare today. Functions with >= 1 equal case:

| size | functions | baseline synth | + class-matched `a0` | baseline, and >= 1 case changed memory |
| --- | --- | --- | --- | --- |
| < 100 B | 5,547 | 4,508 (81 %) | 4,524 | 2,476 |
| 100-300 B | 4,174 | 2,695 (65 %) | 2,723 | 1,333 |
| 300-1000 B | 802 | 481 (60 %) | 489 | 222 |
| >= 1000 B | 47 | 20 | 19 | 11 |
| all | 10,570 | 7,704 (73 %) | 7,755 (+51) | |

Inside vtable runs (1,429 functions) a right-class `a0` lifts 924 -> 966 (+42). No new mismatch appeared (1 vs 2 over
the whole set, both pre-existing patterns).

## What the numbers say

- "The synthetic gate cannot compare them" is true for roughly a third of the functions, not most. Realistic arguments
  of the kind proposed (class-matched objects, caller/sibling frames) add about **+0.5 % of functions** to what is
  comparable and cap at **~13 % of the remaining unreached targets** even if every one of them worked.
- Half of the remaining unreached targets have no caller; no recording can serve them. They need either a route that
  reaches the mode that uses them (Part 2) or to be treated as dead/library code, verified by plain synthetic cases.
- The likelier cause of "1–8 verifiable per PR" is not "no comparable case" but **how deep the compared cases go**
  (the decomp workflow wants the branches real play reaches; "equal" on an early null-check return does not count, and
  only ~46 % of the 300-1000 B functions with an equal case also changed memory in any case). Realism of pointers helps
  that a little; steering inputs by what the *original* actually executed would help it directly.
  This was not measured here (needs `tools.decomp_gate coverage` over a sample); it is the first thing to measure next.

## Options (for the user to pick)

| | what | gain (measured/ceiling) | cost |
| --- | --- | --- | --- |
| A | **Stop here**: keep synthetic cases as they are; put the effort in Part 2 routes and branch-depth measurement | n/a | none |
| B | **Class-matched `a0`** in the case generator (productise the spike: vtable runs from the ELF, objects by vptr, pytest for the run finder, C++ test for the picker) | +51 functions comparable now; +42 in vtable runs | small (~1 day), low risk |
| C | **Donor-triggered cases**: during a route, when a donor G (vtable sibling, static caller, address-taker of F) is entered, the harness also runs F's synthetic cases from that live state with `a0` = G's `a0`; no recording format; a tool lists donors per function | ceiling 749-910 targets; expected comparable share unknown, would be measured first on the 160-function instance sample | medium; reuses the isolated-run machinery |
| D | **Record and replay of memory snapshots** (the idea as stated): serialise registers + closure of pointed-to memory at donor calls, replay in a synthetic phase | same ceiling as C, but the heap moves between route and replay so pointers inside records must be relocated or the memory restored wholesale | high; game-derived data files in gitignored dirs; I do not recommend it over C |
| E | **Coverage-guided inputs**: run the original on a case, read which instructions/branches it executed, mutate arguments toward unexecuted branches; stop per function at a budget | attacks the depth problem directly; unmeasured | medium-high; needs per-instruction hit data from the recompiled original |

Recommendation: **B now** (cheap, measured), **measure branch depth** of a sample of synthetic-verified functions,
then decide between **C** and **E** with that number; **do not build D**. Part 2 (new routes) is the only way to
serve the ~2,800 functions nothing calls and is independent of this spec.

## Design of B (the part proposed for approval)

- `tools/` (Python, TDD): `tools.vtable_runs` reads `.rodata`/`.data` of the ELF and returns runs of >= 2 consecutive
  function addresses (function list from `functions.tsv`); output a gitignored table `recomp/analysis/vtable_runs.tsv`
  (start, count). Pure function + CLI; tests with hand-built sections.
- Host: `GT_SYNTH_RUNS=<file>`; at the start of the synthetic phase group the live objects by vptr, and for a function
  inside a run set `a0` to an instance whose vptr lies in that run (index by case number + seed, deterministic), leaving
  the other registers as today. Functions with no instance keep today's inputs. C++ unit test in
  `host/tests/synth_case_test.cpp` (deterministic, always in the run, falls back when none).
- Gate: `tools.decomp_gate synth` passes the variable; the report says how many functions had a class-matched case.
  A function counts as verified by the same rule as today (>= 1 equal, 0 mismatch, `sabotage --synth` detects it).
- Sabotage: flip a result in a class-matched function that today has no equal case; it must now be detected.
- Full gate and boot check as always; coverage of `tools/` >= 80 %.
- Not in scope: C, D, E, new routes, changing what "done" means.

## Spike code and reproduction

Committed only as evidence, to be reduced or removed once a design is approved: `host/probes.cpp` (`live_scan`),
the hook in `host/gt_kernel.cpp`, `GT_SYNTH_CLASS` in `host/diff_harness.cpp`, and `tools/spikes/recorded_args/`
(`base.py` reached/decompiled sets, `static.py` references and vtable runs, `dyn.py` live instances and synthetic
results; they read `recomp/analysis/spike/`, `probe_pbase_*` logs of the main checkout and the two full synthetic logs).
Commands: `GT_LIVE_SCAN=<file> GT_PAD_EXIT_AFTER=600 GT_PAD=... build/gt_native --decomp off` per route;
`GT_SYNTH=8 [GT_SYNTH_CLASS=1] GT_PAD=<dealer> build/gt_native --decomp diff` for the synthetic runs.
Caveats: one seed, one scene (dealer), 8 cases; the live-object scan covers heap objects with a vptr (static `.bss`
objects and plain C structs are not covered); route timing varies the object set.
