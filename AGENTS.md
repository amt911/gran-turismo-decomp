# gran-turismo-decomp — Agent Guide

A decompilation of **Gran Turismo (PSP, 2009)**, European release **UCES-01245 v2.00**: turning the
game executable (`EBOOT.BIN`, module `PDIAPP`) back into **readable C/C++ that compiles and runs on
other platforms** (PC first). The goal is a decomp — source a person can read and change — not a
byte-identical rebuild; correctness is proven **differentially** against the original code (see
*Strategy*). **The end goal is moddability**: source with real names, types and structure that a
person can change to add mods. The recompiler is an intermediate step towards that, never the
product. Personal research project by Andrés; no game data is ever stored in the repo.

**This file is `AGENTS.md`, read by every agent.** Codex reads it directly; Claude Code reads
`CLAUDE.md`, which only imports it. Edit rules here (or in their `docs/agents/` document), never in
`CLAUDE.md`. Budget: **≤ 32 KiB** (`wc -c AGENTS.md`) — detail goes to `docs/agents/`.

## Start here

- **Where the project stands:** [`docs/ROADMAP.md`](docs/ROADMAP.md) — phases, what is done, what blocks.
  Phase 0 (preparation) is done, including the compiler search (exact Sony GCC builds not public →
  strategy B, decided by the user 2026-10-09). Nothing has been recompiled or decompiled yet.
- **Read [`docs/FACTS.md`](docs/FACTS.md) first** — verified facts about the dump and the binary
  (hashes, sections, compilers in `.comment`, `-G0`, imports). Append when you verify something new.
- **Read [`docs/FINDINGS.md`](docs/FINDINGS.md) before debugging or touching the build** — non-obvious
  gotchas. **Convention:** when you discover something non-obvious that cost time and isn't deducible
  from the code or the binary, add a short entry.
- **Before any binary analysis, read [reverse engineering](docs/agents/reverse-engineering.md)** — REA
  MCP + Ghidra (ghidra-allegrex), PPSSPP, and how a name earns its place.
- **Before decompiling a function, read [the decomp workflow](docs/agents/decomp-workflow.md).**

## ⚡ graphify — scoped, every session

```text
/graphify docs            # first run — docs/ only while there is no src/
/graphify docs --update   # incremental
/graphify query "<question>"    # instead of opening many files
/graphify explain "<name>"      # locate a concept or symbol
```

Once `src/` and `include/` exist, run it over those plus `docs/`. **Never point graphify at `asm/`,
`build/`, `extracted/` or `game_files/`**: tens of thousands of generated `.s` files (or copyrighted
data) bury the graph and cost tokens for nothing. Outputs in `graphify-out/` (gitignored).

## ⚡ superpowers — use whenever applicable

Always prefer **superpowers** skills over ad-hoc approaches. If there's even a small chance a skill
applies to the task, invoke it via the `Skill` tool before acting (including before clarifying
questions).

- **Process skills first** — `brainstorming` before creative/feature work, `systematic-debugging`
  before fixing bugs, `test-driven-development` before writing implementation.
- **Then implementation skills** — domain-specific skills guide execution. Here that is mainly
  `reverse-engineer-anything` (REA) for binary questions.
- **Verify before claiming done** — `verification-before-completion` / `requesting-code-review`
  before merging.

Flow: `brainstorming → spec (you approve) → writing-plans → plan (you approve) →
subagent-driven-development → finishing-a-development-branch`. **Nothing is implemented without an
approved spec.** In this repo a "feature" is a phase or a tooling piece (the GT recomp profile, the
differential harness, a host-layer library, a tool script); **decompiling functions inside an
approved phase does not need a new spec per function** — the differential gate is its spec.

User instructions always take precedence over skills; skills override default behavior. **Skills
refine *how* the work is done; they never override the rules in this file. When a skill and this
`AGENTS.md` conflict, this file wins.**

### Mode switch

- **"lite mode"** — fully disables superpowers: no skill is invoked, not even the applicability
  check, until **"normal mode"** is said.
- **"normal mode"** (default) — standard superpowers behavior, plus: when delegating coding work,
  dispatch at most 1 agent at a time, and never use a model above Sonnet (no Opus). The cap counts
  **implementation** agents: a read-only review agent runs alongside one, and should — see
  [Agent orchestration](docs/agents/agent-orchestration.md#agent-orchestration--parallel-where-its-free-batched-where-its-yours).
- **"modo desatendido"** (unattended mode) — the user is away and delegates autonomy: work without
  waiting for confirmations and make reasonable decisions yourself instead of asking. In this mode you
  MAY **`git push` the feature branches you create** and **open PRs via `gh`** on your own, so the
  work is ready for review when the user returns. The hard limits still hold and are NOT lifted:
  **never merge anything** (no `git merge`, no fast-forward integration, no `gh pr merge`), **never
  push to `main`** or any protected/default branch directly, and **never** `git push --force` /
  `--force-with-lease`. Deliver everything as pushed branches + PRs for the user to merge. Reverts to
  defaults on **"normal mode"**.
  **Pace in this mode:** intermediate tasks run only the differential tests of the functions they
  touched plus the tool tests they touched; commits pile up locally and the branch is pushed **once,
  at the end**, after one full build + full differential pass + boot check.

- **"modo pocos tokens"** (low-token mode) — the user's token quota is short (it resets Wednesdays 19:00): same
  quality and gates, fewer tokens, slower is fine. No subagents (the work and a separate self-review pass done inline, with the same skills),
  terse messages, no exploratory work beyond the priority the user set, batched tool calls, long output to files
  (read the tail), progress artifacts only per finished batch. Ends on **"normal mode"**.

Confirm the switch briefly when it happens.

---

## Rules by topic — what always binds, and where the detail lives

**The lines below bind even if you never open the document; open it before working on that topic.**
A rule is edited in its document, not here and there at once — except for its one-line summary here.

- **Decomp workflow** → [docs/agents/decomp-workflow.md](docs/agents/decomp-workflow.md), before
  decompiling. A function is *done* only when its differential test passes against the recompiled
  original and the game still boots to the reference state; recompiler output is never "decompiled".
  Functions land in **batches of ~80** (hand-written singletons with synthetic cases: **15–25 per PR**): fast gate (`GT_DECOMP_ONLY` + uncapped) while iterating,
  sabotage (s0 flipped at entry), full gate before the PR, routes in parallel (§ Working in batches).
- **Toolchain** → [docs/agents/toolchain.md](docs/agents/toolchain.md), before installing or calling a
  tool. Every tool pinned (version or commit) and fetched by `tools/setup.sh`, never vendored; compilers
  never committed.
- **Reverse engineering** → [docs/agents/reverse-engineering.md](docs/agents/reverse-engineering.md).
  REA/Ghidra output is pseudocode and inference, never "the source"; a symbol name needs evidence
  written next to it.
- **Testing** → [docs/agents/testing.md](docs/agents/testing.md). The differential gate is the test of
  the decomp; `tools/` Python is TDD with pytest, coverage ≥ 80 %.
- **Runtime verification** → [docs/agents/runtime-verification.md](docs/agents/runtime-verification.md).
  Every build change must still boot to the reference states, compared against PPSSPP running the
  original; a check that has never failed is not proven.
- **Debugging** → [docs/agents/debugging.md](docs/agents/debugging.md), before chasing a bug or a
  stubborn mismatch. Measure before you ablate; more than three reproductions → a shortcut script
  before the fourth; a review finding is not a reproduction.
- **Agentic PR verification (mandatory)** → [docs/agents/pr-verification.md](docs/agents/pr-verification.md).
  Every PR gets a verdict comment from a pass that rebuilds from a clean tree, runs the differential
  gate and the boot check; it never merges.
- **Agent orchestration** → [docs/agents/agent-orchestration.md](docs/agents/agent-orchestration.md).
  Review in parallel with the next implementation; shared `docs/FACTS.md`; one agent per worktree
  (the build dir, the REA session, the Ghidra project and each emulator/game instance are exclusive).
  Real-time gates and perf measurements take the CPU lock shared with other sessions (`tools/cpu_lock.sh`:
  30-min holds; orphaned holders skipped); builds and tests never do.
- **Reuse first** → [docs/agents/reuse-first.md](docs/agents/reuse-first.md). Search `include/`,
  `config/symbol_addrs.txt` and FACTS before declaring a type, a struct or a name.
- **Codex and Claude Code** → [docs/agents/agent-compatibility.md](docs/agents/agent-compatibility.md).
  Rules are edited in `AGENTS.md` (or its `docs/agents/` document), never in `CLAUDE.md`.
- **Not applicable here — decided, written so it isn't re-litigated:** UI/UX workflow, E2E with
  Playwright/Maestro, deploy/Docker, SOLID for the decompiled code (it reproduces the original's
  structure, warts included; SOLID applies to `tools/` only), mutation gate (deferred until `tools/`
  has core logic worth mutating — then 60 % over `tools/`, as a ratchet).

## 🧠 Heavy jobs run inside a memory cgroup (MANDATORY)

**No exceptions:** any long or parallel job started here — Ghidra `analyzeHeadless` / REA analysis of
the 7 MB ELF, PSPRecomp code generation over the whole EBOOT, compiling the generated C++ (hundreds of
thousands of lines), the full differential suite, splat, `decomp-permuter`, the pytest suite with
workers, anything that spawns workers — runs under a kernel-enforced memory ceiling:

```bash
systemctl --user set-property --runtime gt-heavy.slice MemoryHigh=5G MemoryMax=6G MemorySwapMax=0
systemd-run --user --scope --quiet --slice=gt-heavy.slice -- <command>
```

**One slice caps all heavy jobs together**, so up to **two** may run at once (two slots,
`tools.decomp_gate.HeavySlot`; `tools.decomp_gate` does all of this itself). Measured 2026-10-13:
an incremental clang build at `-j4` peaks at 0.86 GB, a fresh **gcc** build at `-j8` at 3.76 GB
(~470 MB/job — **configure builds with clang**, `-DCMAKE_CXX_COMPILER=clang++`, as `docs/COMPILAR.md`
does), `tools.recomp` at **3.8 GB** (so it takes both slots: `HeavySlot(exclusive=True)`), a gate route
~0.5 GB (506 MB, 2026-10-21; the full gate keeps at most 7 alive, `MAX_PARALLEL_ROUTES`: memory-bound; each uses ~0.65 cores since 2026-11-05). Two gates need two worktrees (the build dir is exclusive). The slice holds (a 7 GB
allocation stalled at 5.4 GB) — but **at `MemoryHigh` it throttles, and a throttled gate's real-time
routes time out**: a gcc build + recomp + a gate together made every route of a full gate exit 124. A
route that times out or prints no report is a gate failure (`route_problem`), never a pass.

**6 GB is the standing ceiling on this machine** (raised from 4 GB by the user on 2026-08-11); don't
exceed it without being told to. `MemoryHigh` throttles and reclaims, `MemoryMax` is the hard stop,
`MemorySwapMax=0` keeps the job from thrashing swap instead of respecting either. Verify it is
actually in force rather than assuming:
`systemctl --user show <scope> -p MemoryMax -p MemoryHigh -p MemoryCurrent`.

**Cap the tool too — but never *instead* of the cgroup.** Pass the tool's own concurrency limit
(`ninja -j`, `cmake --build -j`, `permuter -j`, `pytest -n`, Ghidra `MAXMEM`) so the job isn't throttled to a crawl by
the ceiling. A tool's default concurrency is not a budget, and an estimate of per-worker RSS is not a
ceiling. Only the cgroup is.

**Check that the ceiling reaches the process that does the work.** A job wrapped in the scope can
hand the real work to a **daemon or worker pool that lives outside it** — the REA MCP server and the
Ghidra JVM it starts are children of the agent, **not** of your scope, and so is a PPSSPP instance
started earlier. The wrapper still reports the limit as applied, over a process that isn't doing
anything. The tell is `MemoryCurrent` sitting near zero while the machine swaps. Confirm against the
**worker's** cgroup, not the scope's: `cat /proc/<worker-pid>/cgroup`. If the worker is outside,
either kill the daemon so the job starts its own inside the scope, or configure the daemon's own
limit — a wrapper that reports success over an idle process is worse than no wrapper, because it
buys confidence and delivers nothing.

**Why this is a rule and not advice:** a mutation-testing run on this 24-core box sized its worker
pool from the core count and spawned **23 workers at ~2.3 GB each** — ~50 GB of demand on 31 GB of
RAM. It took the whole machine down hard enough that the user had to power-cycle it; `systemd-oomd`
did not save it. The run before that was wasted too: with the machine starving, **139 of the first
142 mutants "timed out"**, and a timeout is scored as *killed*, so the result came out inflated by
starvation and meant nothing. A job that OOMs the box doesn't merely fail — it also hands you
numbers you'd trust by mistake.

---

## Legal & data hygiene (non-negotiable)

- **Nothing copyrighted enters git**: no ISO/CSO, no `EBOOT.BIN`, no decrypted ELF/PRX, no
  `GT.VOL` contents, no extracted assets, no RAM dumps, no screenshots of game data in docs. The user
  supplies their own dump in `game_files/` (and their save, `game_files/UCES01245-GAMEDAT`, for the `save_*` routes; the
  game's savedata key and the KIRK keys are never written to the repo); everything derived from it lives in gitignored dirs
  (`extracted/`, `recomp/`, `asm/`, `assets/`, `build/`, `expected/`). `.gitignore` already covers them
  — run `git status --ignored` before a commit that touches new paths.
- **Recompiler output is game-derived, not ours**: the C++ PSPRecomp generates from the EBOOT is a
  mechanical translation of copyrighted code. It is generated at build time from the user's dump into
  `recomp/` and **never committed**. Only hand-written code (decompiled functions, host layer, tools,
  config) enters git.
- **No proprietary toolchain binaries in git** (Sony SDK GCC, SN `pspsnc`, `mwccpsp`). They live in
  `tools/compilers/` (gitignored) with their provenance written in
  [toolchain](docs/agents/toolchain.md); a compiler with unknown provenance is not used.
- **The base dump is verified by hash, not by name**: ISO SHA1
  `c84473375e3cd000d8f14c5905e6514389fd6a40`, decrypted EBOOT SHA1
  `8746583fa8298f6447132988a01b236a8c5994b0`. Tooling refuses other inputs; a different dump is a
  different target and needs its own config.
- **Third-party code keeps its licence and provenance**: PSPRecomp and lcs-recomp are MIT; code taken
  from them (host layer, HLE) keeps the copyright notice and is listed in `docs/agents/toolchain.md`.
  Their own bundled third-party code (FFmpeg, SMAA…) is pulled as a dependency, not copied.
- **Publishing** (pushing a public repo, posting decomp.me scratches) is the user's call, every time.

## Target — what we know (detail and evidence in [FACTS](docs/FACTS.md))

| Item | Value |
| --- | --- |
| Executable | `PSP_GAME/SYSDIR/EBOOT.BIN`, encrypted `~PSP`, tag `0xD9160BF0`, module `PDIAPP`, devkit `0x05050010` |
| Decrypted | ELF `0xFFA0` (relocatable PRX), Allegrex, entry `0x10c`, 7,057,972 B, **section headers with names kept**, **no `.symtab`** |
| Code | `.text` `0x28`–`0x4767dc` (4.46 MiB), ~28k `jr $ra`; VFPU only 0.18 % of instructions |
| Data | `.rodata` + 27 `.rodata.NNNN`; `.data` `0x4d8500`; `.bss` `0x4dd400` + 104 `.bss.NNNN` |
| Language | C++ (`.ctors` 334 entries, `.eh_frame`, `.gcc_except_table`) mixed with C (SGX sound lib `sgx*.c`); no game RTTI |
| Compilers in `.comment` | GCC 3.3.6 allegrex-2.2.2 psp-1.14.0 · GCC 3.3.6 allegrex-2.2.2 psp-1.15.3.internal · GCC 4.0.4 allegrex-2.2.3 psp-2.8.2 |
| Codegen | `-G0` (zero `$gp`-relative accesses), EABI32, `long32` |
| Imports | 36 Sony libraries, 329 functions (NIDs in `.rodata.sceNid`) |

**Scope.** In: the native code of `PDIAPP` (game + statically linked engine). Out (other projects
already cover them or they are Sony's): the **Adhoc scripts** in `GT.VOL` — Nenkai's
[OpenAdhoc](https://github.com/Nenkai/OpenAdhoc) reports GT PSP (`gt5m`) at 100 % — the **assets** in
`GT.VOL` ([GTPSPVolTools](https://github.com/Nenkai/GTPSPVolTools)), the Sony PRX modules in
`USRDIR/MODULE`, and the firmware updater in `SYSDIR/UPDATE`. Statically linked Sony SDK code inside
`PDIAPP` (libc, libstdc++, `sce*` stubs, SGX) is identified and kept as asm or as a library unit — it
is not a decompilation target until the game code is.

## Strategy & phases (status lives in [ROADMAP](docs/ROADMAP.md))

**Decided by the user on 2026-10-09 — strategy B: recompiled baseline, then decompile on top of it.**
The exact Sony compilers are not public (search and tests in
[toolchain](docs/agents/toolchain.md#compiler--searched-not-found)), and the goal is source that
compiles on other platforms, so byte-matching is not the gate. Instead:

1. **Reproducible extraction** — `tools/` verifies the ISO hash, extracts, decrypts, verifies the ELF
   hash.
2. **Recompiled baseline** — a GT profile for [PSPRecomp](https://github.com/jessicanataliagta/PSPRecomp)
   (static Allegrex → C++ recompiler behind the GTA LCS/VCS PC ports) turns the whole EBOOT into C++
   that runs natively, over a host layer that implements the Sony imports (151 of GT's 329 already exist
   in PSPRecomp's VCS profile — FACTS). The game runs on PC before a single function is decompiled.
3. **Differential harness** — any function can be swapped between its recompiled original and its
   decompiled version; both run on the same captured inputs and must leave the same outputs and memory
   side effects. This harness is the gate that replaces the checksum.
4. **Function-by-function decompilation** — replace recompiled functions with readable C/C++, leaves
   and libraries first, then game code by subsystem, following the
   [decomp workflow](docs/agents/decomp-workflow.md). Progress = share of functions (and bytes) that
   are decompiled and pass the gate.
5. **End state — no emulation of any kind** (user, 2026-10-15; see below), then other platforms, and a
   **matching track** if the exact compilers ever surface: `psp-gcc-1.7.1` already reproduces some
   functions byte for byte, so matching stays possible per function as extra evidence, never as the gate.

**End state: 100 % decompiled, no emulation** (user, 2026-10-15). Everything PSP-shaped that runs today
is scaffolding to remove, in this order, each step keeping the differential gate green:
1. **No recompiled code** — every function decompiled (the gate compares against the recompiled
   original until the last one is gone; then the decompiled build is the reference).
2. **No emulated memory** — guest addresses (`word(rt, self + 0x1C)`, `call_guest`) become C++ types:
   structs, pointers, classes, containers. This is also the naming/typing work that makes mods possible.
   While both kinds of code coexist they must share one memory layout, so the move goes by subsystem,
   from the leaves up, behind typed accessors.
3. **No PSP interfaces** — the game's renderer calls the host GPU API instead of building GE command
   lists, sound and input go straight to the platform layer, and the Sony libraries (HLE today) become
   that platform layer, as in a PC port.
Write new decompiled code so this is easy: typed helpers and named fields instead of raw offsets where
the evidence allows, no new dependence on recompiler internals. The plan (stages, how the gate survives
each one) is its own spec, to be written and approved before any code (planned:
`docs/superpowers/specs/*-no-emulation-design.md`).
**For a new decompilation project, reuse the path:** recompiled baseline that runs → per-function
differential gate against it → decompile leaves first, in batches → retype the memory → native platform
layer. Read [lessons](docs/agents/lessons.md) and FINDINGS before planning; they are the expensive part.

**The goal is the decomp, and through it mods** (user, 2026-10-09). The recompiler is scaffolding:
the measure of progress is decompiled, gate-passing, *understandable* source — named functions,
typed structs, known file boundaries — never "it runs". A choice that makes the game run sooner but
leaves the code harder to read or change (e.g. patching recompiled output) is the wrong trade. A phase whose design is still open (the interop between
decompiled code and guest memory, the capture format of the harness) gets its own spec before code.

## Repository layout (planned — create a directory in the phase that needs it)

```text
/
├── AGENTS.md / CLAUDE.md / README.md
├── .mcp.json             # REA MCP registration with GHIDRA_INSTALL_DIR (Claude Code)
├── config/               # symbol_addrs.txt, checksums (*.sha1), recomp profile config (function list, overrides)
├── include/              # game structs and types, PSP SDK decls, guest-memory access helpers
├── src/                  # decompiled C/C++, one file per original translation unit when known
├── host/                 # host layer: Sony imports (HLE), GE renderer, audio, input — ours or MIT-derived
├── tests/differential/   # harness + captured cases per decompiled function (no game bytes inside)
├── tools/                # setup.sh, extraction & helper scripts (Python), tests in tools/tests/
├── docs/                 # ROADMAP, FACTS, FINDINGS, agents/
├── game_files/           # (ignored) the user's ISO
├── extracted/            # (ignored) ISO contents, EBOOT.elf
├── recomp/               # (ignored) C++ generated by PSPRecomp from the user's EBOOT
└── build/                # (ignored)
```

The split between `src/`, `host/` and `recomp/` is the Phase 2/3 spec's to settle; the rule that
`recomp/` is generated and ignored is not.

## Commands

**Only what exists is listed as runnable.** A command that a later phase will create is marked
*(planned)* and must not be presented as working.

```bash
# Phase 1 — verified on 2026-10-09
tools/setup.sh [--with-ghidra]          # pinned tools → tools/, .venv/; idempotent (2nd run ~0.3 s)
.venv/bin/python -m tools.extract       # ISO hash → 7z (no GT.VOL) → pspdecrypt → ELF hash; idempotent
.venv/bin/pytest --cov --cov-fail-under=80   # tool tests (+ the real-dump test when the ISO is present)
# Phase 2a/3 — verified on 2026-10-10 (full guide: docs/COMPILAR.md)
# .venv/bin/python -m tools.recomp; cmake --build build -j8   (inside the cgroup)
# build/gt_native --max-seconds 30 --trace recomp/analysis/gt.trace [--decomp off|on|diff]
# .venv/bin/python -m tools.trace_compare recomp/analysis/ppsspp.log recomp/analysis/gt.trace
# tools/gpu_gate.sh build recomp/analysis/gpu/run [SCALE] ["race:600 ..."]   GPU vs software GE gate (gt_play headless)
# ctest --test-dir build
# tools/mod_pedals_check.sh [--without-mod|--sabotage|--toggle-off] [--decomp off|on]   analog-pedals mod in-game check (needs ../gt-mods; docs/MODS.md); --toggle-off = the F9 menu's toggle mid-race
# tools/mod_camera_check.sh [--without-mod|--sabotage|--toggle-off] [--yaws "0 90 180"] [--decomp off|on]   orbit-camera mod in-game check on the `chase` route (frames vs a run without the mod; needs ../gt-mods; docs/MODS.md)
# tools/mod_reset_check.sh [--without-mod|--sabotage-rates|--sabotage-newest|--toggle-off] [--decomp off|on]   reset-to-track mod check (../gt-mods; docs/MODS.md)
# .venv/bin/python -m tools.disasm 08B54FB8 [ADDR:N]   asm of a function: relocations applied, branches as `-> ADDRESS` (read targets here, never count them by hand)
# .venv/bin/python -m tools.frame_audit [FILE.cpp ...]   body GuestFrame sizes vs the original prologue (the gate can't see them)
# Ghidra 12.1.3 + ghidra-allegrex, headless (~154 s, inside the memory cgroup):
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  env JAVA_HOME=/usr/lib/jvm/java-27-openjdk MAXMEM=4G \
  tools/ghidra/ghidra_12.1.3_PUBLIC/support/analyzeHeadless ghidra gt -import extracted/EBOOT.elf \
  -overwrite -max-cpu 4 -scriptPath tools/ghidra_scripts -postScript Phase1Probe.java

# (planned) recomp step         PSPRecomp over extracted/EBOOT.elf → recomp/ (GT profile)
# (planned) cmake --build       game binary = recomp/ + src/ + host/
# (planned) differential suite  run every decompiled function against its recompiled original
# (planned) progress report     decompiled + passing functions / total, game vs SDK
```

## Decompilation — the rules that bind every branch

The detail (the loop, the harness, m2c, C++ specifics, the optional matching check) is in
[the decomp workflow](docs/agents/decomp-workflow.md).

- **The differential gate is the test suite of this repo.** From Phase 3 on, every decompiled
  function has captured cases and passes them against its recompiled original, on every commit. A red
  gate is a broken build, whatever the C looks like.
- **Never claim a function done without showing the harness result** (cases run, all equal) **and the
  boot check**. "It compiles" and "the logic looks equivalent" are not done.
- **Recompiler output is not decompiled code.** Copying generated C++ into `src/` and tidying it is
  forbidden: it is game-derived, unreadable by construction, and proves nothing. Decompiled code is
  written from the asm/pseudocode by hand (m2c/Ghidra drafts as reading aids).
- **Never weaken the gate to get green** — no deleting or editing captured cases, no widening the
  comparison, no excluding side effects. Fix the C; if a case is wrong, say why in the commit.
- **No guessed names.** Unknown symbols keep `func_XXXXXXXX` / `D_XXXXXXXX` (ELF vaddr). A name needs
  evidence (a string, an import NID, an Adhoc binding, an assert/file name, a decompiled caller) and
  the evidence is written next to it in `config/symbol_addrs.txt` or the header — see
  [reverse engineering](docs/agents/reverse-engineering.md#naming--a-name-needs-evidence).
- **Faithful first, portable second** — same behaviour as the original, including its quirks (a
  "fix" changes the game and fails the gate). Portability problems (pointer size, endianness, VFPU,
  undefined behaviour the PSP tolerated) are solved explicitly in types and helpers, with a comment,
  never by silently changing logic.
- **Tool code is real code**: anything in `tools/` follows TDD with pytest (≥ 80 % coverage); see
  [testing](docs/agents/testing.md).

## Reverse engineering — the rules that bind

Detail in [reverse engineering](docs/agents/reverse-engineering.md).

- **REA cannot open the EBOOT** (REA 6.2.0 admits only x86/x86_64/ARM/ARM64 ELFs — FINDINGS). For
  the game binary use **Ghidra 12.1.3 + ghidra-allegrex directly** (headless or GUI). Keep REA's
  discipline anyway: observations, inferences and unknowns apart; cite where a claim came from. Use
  REA (`reverse-engineer-anything` skill) for what it does admit, and re-check its ELF support on
  each release.
- **Ghidra needs ghidra-allegrex** for VFPU and PRX relocations; without it MIPS decoding of VFPU
  is wrong. Decompiler output is a reading aid for `src/`, never pasted as decompiled code.
- **Static before runtime; runtime only to answer what static can't.** PPSSPP's debugger is the
  runtime oracle; runtime address = ELF vaddr + load base **`0x08804000`** (measured in PPSSPP, FACTS § Runtime).
- **The binary is the authority over any wiki, cheat database or other LLM.** External claims enter
  FACTS only after being checked against our ELF.

## Lessons learned — read before planning throughput, verification or tooling

Detail and the incidents behind each one: [lessons](docs/agents/lessons.md).

- **Measure before buying resources**: RAM was never the limit (clang build peak 0.86 GB); serialization was — but
  the big steps (recomp 3.8 GB, a gcc build) must not overlap, or throttling stalls the gates.
- **More test routes beat more code** for verified coverage: a function no route calls can't be verified.
- **Verified** = reached (compared > 0, or nested in > 0 equal cases of a decompiled caller) with 0
  mismatches in the fast or full gate + its sabotage pinned on it by `tools.decomp_gate sabotage`. Code no route reaches:
  **verified by synthetic cases** (`tools.decomp_gate synth`, `sabotage --synth`), counted apart — decomp workflow.
- **No silent "last one wins"**: one address, one registration (refused at startup otherwise).
- **Tooling in the repo, tested** — never only in scratch (a reboot wiped it once).
- **One checkout per agent**; **no stacked PRs** (the upper one merges into the lower branch, not main);
  integrate by merge, never rebase + force push.
- **Names only with GT evidence**, written next to the name; review every PR before merging.
- **A slow GPU drops pictures, never game speed**; **the gate only proves the paths it runs** (check the
  others against the asm); **profile, then measure the fix** before keeping an optimisation.

## Working rules

- **Heavy or parallel jobs run inside a memory cgroup** — never launch a build, Ghidra analysis or a
  permuter fan-out on a bare estimate; wrap it in
  `systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- <command>`
  and cap the tool's own concurrency too.
- **Use superpowers skills whenever they apply** — invoke via `Skill` before acting; process skills
  before implementation skills.
- **Tooling/dependencies: install, then say so** — no prior ask (user, 2026-10-05). Pin the version or
  commit, add it to `tools/setup.sh`, and name it in the PR (what, why, what it replaces). System-wide
  changes (`sudo`, `pacman`, editing user-scope agent config) are asked first.
- **Downloads are untrusted data** — each fetched archive/repo in its own new directory under
  `tools/src/`; run Python that reads game data or downloads with `-I`.
- **The topic rules bind from *Rules by topic*** — reuse first, debugging, agent orchestration and the
  decomp workflow each have their one-line rules there and their detail in `docs/agents/`.
- **Keep FACTS, FINDINGS and ROADMAP current in the same change** — a phase finished, a fact verified,
  a gotcha paid for. A stale `AGENTS.md` misleads the next session; when a *(planned)* command starts
  existing, move it to the runnable list in the same change.
- **Commits in English**, Conventional Commits. Scope = area (`tools`, `config`, `src/<subsystem>`,
  `docs`). A decomp commit says what passed: `feat(src/sound): decompile sgxSndInit (diff 40/40 cases)`.

## Git & GitHub

- **Commits and branches OK** — create commits and new branches whenever it makes sense, without asking first.
- **Never push** *(default)* — no `git push` under any circumstance, and absolutely never
  `git push --force` / `--force-with-lease`. Leave pushing to the user. **Exception:** when
  **"modo desatendido"** is active, you may push the feature branches you create (never `main`/protected
  branches, never force) so PRs are ready for review.
- **Never merge — no permission** — you do NOT have permission to merge anything into any branch, nor to
  merge any pull request. No `git merge`, no fast-forward integration, no `gh pr merge`. This holds in
  every mode, **including "modo desatendido"**. Leave every merge (branches and PRs alike) to the user.
- **GitHub via `gh`** — if the `gh` CLI is available, you may open pull requests, issues, and similar
  (comments, labels, etc.). These don't require pushing on your part beyond what `gh` itself does for an
  already-pushed branch.
- **Branches:** `feat/name`, `fix/description`, `chore/task`, `decomp/<subsystem>` for decompilation work.
- **Every PR must include a manual test plan** — a **How to test manually** section with the exact
  steps to exercise the change by hand. Here that means: the commands to run from a clean clone with the
  user's ISO in `game_files/` (setup, extract, recomp, build, differential suite), the expected output
  (hashes, the harness results of the touched functions, the progress delta), and how to boot the
  build and which reference state to reach.
