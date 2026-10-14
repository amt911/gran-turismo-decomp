# Lessons learned (2026-10)

What this project paid for, kept so it isn't paid twice. The same text, written for any project, is in the
user's global notes (`~/.claude/lessons/decomp-recomp-projects.md`). Each lesson names where the rule now
lives in this repo when it became one.

## Throughput — what actually limits it

- **Measure before buying resources.** The memory a job *seems* to need is rarely the bottleneck. Read
  the real peak (`systemd-run --user --wait …` prints it; or `systemctl --user show <unit> -p MemoryPeak`)
  before raising a limit. Here a full build peaked at 0.9 GB under a 6 GB cap: more RAM would not have
  helped (AGENTS.md § Heavy jobs). The real limits were serialization (one global lock for every heavy job) and gates that run in
  real time.
- **A shared ceiling still needs the big jobs kept apart.** Below `MemoryMax` the slice doesn't kill, it
  throttles at `MemoryHigh` — and a throttled real-time test times out. Know each heavy step's peak
  (here recomp 3.8 GB, a gcc build 3.76 GB vs clang far less) and give the biggest one the whole slice.
- **A run that timed out or printed nothing is a failure, not "0 mismatches".** The gate tool once
  summed every route of a run — all empty, all timed out — into "0 mismatched, rc=0".
- **Cap the total, not each job.** Put all heavy jobs in one systemd slice with the memory ceiling
  (`--slice=<name>.slice`, `MemoryMax` on the slice) and let several run at once. A per-job cap plus a
  global lock gives safety but serializes everything; a shared slice gives the same safety in parallel.
- **The gate's fixed cost dominates small changes.** A run costs the same for 1 function or 100: batch
  the work so each gate run carries enough.

## Verification — a gate is only as good as what it reaches

- **A function the test routes never call cannot be verified.** Track "written but unverified"
  separately. The cheapest way to raise verified coverage is often **more test routes** (input scripts
  that reach menus and states the current ones skip), not more code.
- **Define "verified" precisely and write it down** (decomp-workflow.md § Working in batches): compared > 0 with 0 mismatches in *any* gate run
  of this change, *and* a sabotage of that function detected. Faster/uncapped runs often reach code the
  real-time run doesn't; requiring the slow run alone throws away verified work.
- **Every check must have been seen failing.** Sabotage each function (a change every comparison sees,
  e.g. bump a callee-saved register at entry) and require the gate to catch it.
- **Silent "last one wins" is a bug factory** (`gt::duplicate_registration`, PR #130). If two registrations / definitions of the same thing can
  coexist, make it a hard error at startup. Here an older copy silently shadowed renamed functions, so
  their new code never ran while the gate stayed green.
- **Nested calls inside a test case may not exercise the callee's new code** (the harness may run
  originals for nested calls). Know your harness's rule and don't count a child as verified because its
  parent passed. Here: only `GT_DECOMP_NESTED` callees run decompiled when nested (decomp-workflow.md).
- **Verify helpers through their callers, and attribute exactly.** Running nested decompiled callees
  decompiled inside a case lets the gate reach helpers no route calls directly — but then a parent's
  mismatch may be its callee's fault: log each mismatching case's members and credit a sabotage only where
  it was the one sabotaged function; re-run parents and callees that always fail together in separate rounds.
- **A sabotage must survive callers.** Bumping a callee-saved register is erased by any caller that saves
  and restores it; bump a compared memory word nobody uses instead (or as well).
- **Independent review catches what the author's gate can't**: wrong comments, invented names, weak
  evidence, mis-stated numbers. Review every PR before merging, even when all gates are green.

## Readability — the goal is source people can change

- **Mechanical batches raise the count; named subsystems raise the value.** Loose `func_XXXXXXXX`
  functions are hard to mod. Alternate: batches (with automatic drafts for repeated shapes) for volume,
  subsystems (typed structs, real names, one file each) for quality.
- **Names only with evidence, evidence written next to the name.** The best evidence is the program's own
  strings (script bindings, registration tables, asserts). A role-derived name ("block_index") is a
  guess: keep `unk_XX` / `func_XXXX` with the role in a comment.
- **Typed views over guest memory** (layout structs + accessors that go through the emulated memory) make
  decompiled code readable without assuming host pointers, so it stays portable.

## Process hygiene

- **Tooling the workflow depends on lives in the repo** (`tools/decomp_gate.py`), tested — never only in a temp/scratch directory.
  A reboot wiped /tmp mid-PR and took every gate script with it.
- **One checkout per agent.** Never `git checkout`/switch branches in a working tree another agent is
  using: its uncommitted work moves with you and its branch changes under it. Make your own worktree.
- **Don't stack PRs on each other's branches.** Merging the upper one merges into the lower branch, not
  into main; the work silently never reaches main. Branch from main, merge main into the branch to
  integrate.
- **Integrate by merge, never by rebase + force push**, when the project forbids force pushes — even on
  your own branches.
- **Counters must agree.** If the progress report and the gate disagree on how many functions are done,
  one of them is wrong; check address by address.
- **Correct your own mistaken instructions to helpers explicitly** (what was wrong, the right rule, what
  to redo) and write the corrected rule into the project docs so it doesn't recur.

## Host / runtime (emulation layer)

- **A slow GPU must drop pictures, never slow the game.** If the end of a frame waits for the GPU and the game
  thread waits for the frame, GPU time becomes game time; GT advances logic and sound per frame, so it ran in
  slow motion with lagging audio — while `GT_PERF_LOG` still read `guest time 1.00x`. Watch `GE wait` and the
  audio rate; reproduce on a fast machine with `GT_SIM_GPU_MS` (FINDINGS; lcs-recomp patch 0026).
- **Debug fallbacks inherited from another port bite.** LCS's "texture not ready → debug colour" painted every
  new texture's first draw: purple videos and fades, a cyan sky (patch 0024). Colours that are wrong *and vary
  between runs* point at a fallback path before a format bug.
- **The gate only proves the paths it runs.** A branch no route takes (a special-case constant, an error path)
  must be compared against the asm by hand: review found `a == 2 → 2` where the original stores 0 (#151).
- **Profile before optimising, then measure the fix.** A software-rasteriser texture cache was exact and cut the
  hottest-looking function but saved only 5-10 %; it was dropped and the reason written down (FINDINGS).

