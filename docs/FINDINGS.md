# FINDINGS — gran-turismo-decomp

This is the repo's **non-obvious gotchas log**. `AGENTS.md` § *Start here* makes it mandatory reading
before debugging or touching the build, and mandatory writing whenever you discover something that cost
you time and **is not deducible from the code or the binary**.

## What goes here

An entry earns its place only if **all** of these hold:

- It cost real time to discover (a wrong path taken, a silent failure, a misleading error).
- It is **not** deducible by reading the code, the binary or the tests.
- It will bite again: it's a property of the toolchain, the binary format or the environment, not a
  one-off typo you already fixed.

Not here: verified-but-deducible facts about the binary (that's [`FACTS.md`](FACTS.md)), how the project
works (that's `AGENTS.md`), or a bug fixed in the same change (commit + regression test).

## Format

One `###` entry per finding, newest first.

```markdown
### <Short symptom, as you'd search for it>

**Symptom.** <What you actually saw.>
**Cause.** <The real mechanism, once known.>
**Fix / workaround.** <The command, flag or code change that resolves it.>
**Why it isn't obvious.** <Why the code doesn't tell you this.>

Found: <YYYY-MM-DD> · <area: extract | split | build | match | re | runtime | env>
```

---

## Findings

### Menu text invisible: glyphs arrive by GE block transfer and are drawn as transformed sprites

**Symptom.** Boxes and icons render, every text is missing; `sceFontGetCharGlyphImage_Clip` writes
correct glyphs into GT's glyph cache.
**Cause.** Two renderer gaps (`GT_GE_LOG=<vblank>` traces a frame's GE lists): GT copies each glyph
from its cache into a 32×32 T4 texture with a GE block transfer (TRXKICK, 3,340 per frame) right
before drawing it, and the LCS executor ignored TRXKICK; and the glyph is a *transformed* sprite
(`through = 0`), which the software rasterizer skipped outright.
**Fix / workaround.** TRXKICK in `host/gt_ge_exec.cpp` (flushing deferred rasterization first);
transformed sprites project both corners and draw the rectangle (lcs-recomp patch 0013).
**Why it isn't obvious.** The atlas texture reads as all zeros when you inspect it at enqueue time —
it is filled by the list itself, one glyph at a time.

Found: 2026-10-10 · rendering

### White screen after the logo — a save GT wrote itself

**Symptom.** Every run suddenly showed a white layout instead of the title and menu, even on old
commits.
**Cause.** GT autosaves; our runs had written `PSP/SAVEDATA/UCES01245-GAMEDAT` into the game
directory, and loading that save ends on a white screen (save loading isn't right yet).
**Fix / workaround.** Saves go to `recomp/savedata`, emptied at every start (`GT_SAVEDATA_DIR` keeps
them). The faulty save is kept in `recomp/analysis/savedata_white_bug` for later.
**Why it isn't obvious.** Nothing in the code had changed; the state lived outside the repo.

Found: 2026-10-10 · runtime

### A harness case let the timer preempt the PSP thread — the game then crawled

**Symptom.** With `Module::clearStatic` decompiled, `--decomp diff` advanced 180 vblanks in 60 s
(normal: ~3,500); `--decomp-cases 0` and `--decomp on` were fine.
**Cause.** Isolated runs dispatch guest code directly, and the runtime's starvation tick still fired
inside them: it advanced the virtual clock, woke sleepers and preempted the current thread
(`timer-preempt`) — real scheduler changes the harness then "undid" only in guest RAM. Two more
holes of the same family: an LwMutex unlock inside a case could wake a waiter (host-side queue), and
a decompiled run's `call_guest` re-entered the harness's own trampolines (cases inside cases).
**Fix / workaround.** `gt::set_kernel_isolated` (`host/kernel_isolation.hpp`): while a case runs —
**both** the original and the decompiled run; isolating only the original later made
`Module.clearStatic` differ by 1,652 bytes, the tick firing inside the decompiled run's
`call_guest` — the starvation tick does nothing, LwMutex lock-with-contention / unlock-with-waiters refuse and mark the
case broken (dropped and replayed for real); nested decompiled calls run their originals; isolated
runs are also bounded to 250 ms.
**Why it isn't obvious.** Every comparison stayed green; only the game's progress showed it, and only
once a function with a long original (a module-wide clear) was sampled.

Found: 2026-10-10 · harness

### `--decomp on` stops: "the callee switched PSP threads" — disposing an object can block

**Symptom.** With the strong release decompiled, `--decomp on` stopped at
`call_guest(0x08A849CC): the callee switched PSP threads`.
**Cause.** The release calls the object's dispose function (vtable slot 2); some disposals wait in
the kernel (a semaphore, an LwMutex under contention). Recompiled code survives that by returning
to the dispatcher with everything in guest registers; a native C++ frame can't.
**Fix / workaround.** Fibers (`host/fiber.cpp`): each decompiled call from the dispatcher runs on its
own native stack; `call_guest` suspends it on a thread switch and resumes it when the thread
returns to the fiber's return address. To the menu: 3 fibers, 41 suspensions, no stop.
**Why it isn't obvious.** In `--decomp diff` the same function never blocked — the sampled calls
happened not to dispose anything that waits.

Found: 2026-10-10 · runtime

### The diff gate passed sabotaged smart-pointer code: all samples came from boot

**Symptom.** `holder_destroy` / `holder_reset` sabotaged (no release, wrong store) still showed
32/32 equal — with 0 "cases that changed memory" although they run 200,000 times to the menu.
**Cause.** Sampling 1 call in 64 with a 32-case budget spends the whole budget in the first ~2,000
calls, i.e. boot, where these calls were trivial (empty holders) — and a fixed stride can lock onto
one call site of a periodic loop.
**Fix / workaround.** Geometric spacing with jitter (each sample ~1.6× the previous call index):
cases now come from the whole run, both sabotages fail the gate. For rare branches,
`GT_DECOMP_DENSE=0xADDR` compares every call of one function (the weak release's delete branch
showed up once in 3,000).
**Why it isn't obvious.** The gate looked green and the case counts looked healthy; only the
"cases that changed memory" column gave it away.

Found: 2026-10-10 · harness

### Title screen crashes reading address 4 — no font glyphs, not a localisation problem

**Symptom.** ~10 s after the GT logo (with or without skipping the movie): `Guest memory access
outside PSP RAM/EDRAM at 0x00000004`, `dispatch=0x0898A6B4` (an `MLocalizedText` widget method).
**Cause.** The `dispatch`/`guest_pc` in the error are stale (direct calls bypass dispatch); the real
fault is in text layout: `FUN_0024ae1c` asks `FUN_0024c288(font, char)` for a glyph, gets NULL and
reads `*(short *)(glyph + 4)`. GT builds its glyph atlas at boot with `sceLibFont`, and the 2a stubs
returned no glyph metrics, so no glyphs existed. Red herrings on the way: `MLocalizedText` asserts
(`FUN_00257b7c` is an empty release-build assert), `rtext.rt2` loading (fine), memory exhaustion (no
`sceKernelAllocMemoryBlock` failed).
**Fix / workaround.** Real `sceLibFont` from the user's PGF fonts (`host/pgf.cpp`, `host/font_hle.cpp`).
**Why it isn't obvious.** The crash report names the wrong function; the NULL comes from a lookup
three calls down.

Found: 2026-10-10 · runtime

### Intro movie: FFmpeg can't open GT's movies from GT.VOL — the bytes there are scrambled

**Symptom.** `sceMpegAvcDecode` fails forever after the GT logo; `GT_MPEG_LOG=1` shows no source.
**Cause.** The LCS-derived sceMpeg decodes from a loose `.PMF` file matched by size. GT keeps its
movies inside GT.VOL, and the 2 KiB `sceIoRead` that loads the PSMF header returns scrambled bytes
(`38 4D 7B 81…`, not `PSMF`): the game descrambles them in guest memory before `scePsmfVerifyPsmf`.
Also, LCS never released ring-buffer packets, so the game stopped feeding the ring after 1,024.
**Fix / workaround.** Decode from what the game puts in the sceMpeg ring buffer: every packet the
ring callback writes is copied to a host queue (`host/movie_decoder.cpp`), demuxed (MPEG-PS,
`host/ps_demux.cpp`) and decoded with FFmpeg's H.264; packets are freed in the ring as they are used.
**Why it isn't obvious.** The header read looks like any other 2 KiB read, and `scePsmfVerifyPsmf`
then succeeds on the same buffer.

Found: 2026-10-10 · runtime

### Render thread spins forever on `sceDisplayWaitVblankStartCB`, no frame ever set

**Symptom.** Only one `sceGeListEnQueue`, then thread `PDI:00000005` calls
`sceDisplayWaitVblankStartCB` every frame and never reaches `sceDisplaySetFrameBuf`; no image.
**Cause.** `FUN_00355a24` (runtime `0x08B59A24`) waits until a frame counter (`0x08D6CE20`) has advanced
N frames. Only GT's VBlank sub-interrupt handler (`sceKernelRegisterSubIntrHandler(30, 0, 0x08B5A330,
0x08D6CE00)`) increments it. The LCS-derived kernel dispatched only sub-handler 15 (the one LCS uses).
**Fix / workaround.** Dispatch every enabled sub-handler of interrupt 30 in sub-number order, chained on
return (`next_vblank_sub_interrupt` in `host/gt_kernel.cpp`). The game then shows its boot screens.
**Why it isn't obvious.** Nothing fails: the thread is alive and waits politely; the trace only shows
"too many vblank waits". Found by Ghidra refs to the `sceDisplaySetFrameBuf` stub back to the loop.

Found: 2026-10-10 · runtime

### Boot crash on a NULL global: a zero-max semaphore was rejected

**Symptom.** `gt_native` died after ~82k import calls loading `0+8` at guest pc `0x08ADB53C`.
**Cause.** The global read there (bss `0x08D40648`) has one writer, an init step chained with `&&`
after GT's SGX sound init (`FUN_002e6d38`). That init creates `sgx-psp-freq-free-sema` with initial 0
and **maximum 0**; the LCS-derived kernel rejected `maximum <= 0`, PPSSPP returns a UID. The SGX init
failed, the chain stopped, the global stayed NULL.
**Fix / workaround.** Accept `maximum == 0` (reject only negative). Found by `GT_PROBE` entry probes +
Ghidra references (`tools/ghidra_scripts/RefsTo.java`) + comparing the SGX init calls with PPSSPP's.
**Why it isn't obvious.** The crash is far from the cause, in a different subsystem.

Found: 2026-10-10 · runtime

### GT's memset writes one byte past the buffer (and that is the behaviour to keep)

**Symptom.** The harness flagged `memset(dst, 0, 9)`: the original also wrote `dst + 9`.
**Cause.** For `count >= 9` the SDK memset stores its tail with `swl value, 0(dst + count)` instead of
`dst + count - 1`; little-endian `swl` covers up to and including `dst + count`.
**Fix / workaround.** `src/libc/memset.cpp` reproduces it (faithful first). Not a harness bug.
**Why it isn't obvious.** It looks like a textbook memset.

Found: 2026-10-10 · decomp

### Entry probes / overrides can be bypassed by direct cross-unit calls

**Symptom.** A `GT_PROBE` on the crashing function counted 0 entries although the crash happened
inside it.
**Cause.** Not fully investigated: generated code calling into another unit can use a direct chained
call that does not go through the dispatcher, so a `register_function` override is not consulted.
**Fix / workaround.** Probes and `--decomp diff` may see fewer calls than really happen (fine for
diff); `--decomp on` may not use the decompiled version on every call — check before relying on it.
**Why it isn't obvious.** The runtime documents that overrides disable chaining into their unit.

Found: 2026-10-10 · runtime

### The differential gate passed a sabotaged memset

**Symptom.** `memset` writing one byte too few: `compared 16, equal 16, mismatched 0`.
**Cause.** Every compared call filled memory that was already zero (boot-time memsets of freshly
allocated, zeroed blocks), so the missing write was invisible. The harness itself works: a sabotaged
return value gives 16/16 mismatches and exit code 2.
**Fix / workaround.** The report now counts `cases that changed memory`; compared cases are spread
over the run (first 4 calls, then 1 in 64). Rule in decomp-workflow.md: a memory-writing function
needs at least one such case before it counts as proven.
**Why it isn't obvious.** "16 equal" reads like strong evidence.

Found: 2026-10-10 · decomp

### PPSSPP's HLE log is not a complete call trace

**Symptom.** trace_compare reported divergences that were not real: calls missing on PPSSPP's side,
names that didn't match, calls the game never makes.
**Cause.** PPSSPP (1.20.4) (a) logs some calls only in some states (e.g. `sceKernelCheckCallback`),
(b) prints symbolic returns (`ILLEGAL_PRIORITY=sceKernel...`), (c) names some stubs with a suffix its
call lines omit (`Importing scePowerSetClockFrequency350` vs `scePowerSetClockFrequency(...)`), and
(d) logs internal pseudo-calls (`__KernelReturnFromMipsCall`).
**Fix / workaround.** `tools/trace_compare.py`: ordered match with bounded gaps (reorderings still fail),
symbolic returns accepted, numeric suffixes normalised, only functions the game imports counted.
**Why it isn't obvious.** The log looks like a call-by-call trace.

Found: 2026-10-09 · runtime

### A GT thread polls the pad 171 million times in 30 s

**Symptom.** 30 s run produced a 3.3 GB trace dominated by `sceCtrlReadBufferPositive` wrapped in
`sceKernelCpuSuspendIntr`/`ResumeIntr`.
**Cause.** The LCS HLE returns pad samples immediately; on hardware a second read in the same vblank
period blocks until the next sample. With a cooperative scheduler the poller starved every thread.
**Fix / workaround.** `host/gt_kernel.cpp`: a repeated read in the same sample period delays to the
next sample (+1 us, so vblank waiters resume first). The period must be the kernel's `kVblankPeriodUs`
(16,683 us, 59.94 Hz): with 1/60 s (16,667 us) samples arrived *before* each vblank and the pad thread
overtook the vblank waiters. `--trace-limit` (default 200k lines) caps trace size.
**Why it isn't obvious.** LCS never hits it; nothing crashes, the run just makes no progress.

Found: 2026-10-09 · runtime

### Generated GT units don't compile: `local_pc` / `entry_id` undeclared, VFPU register `188`

**Symptom.** clang on lcs-recomp's generated units: `use of undeclared identifier 'local_pc'` /
`'entry_id'`, and `static_assert(ScalarRegister < 144u)` failing with `188` and `255`.
**Cause.** (1) Units whose entries span more than 8,192 slots get the *sparse* dispatcher, which
declared neither variable and switched on the stale `ctx.pc`, while `jr` lowering writes both — LCS's
units all fit the dense path, GT's don't. (2) `mtv`/`mfv` take their register from imm8; GT has 3
instructions naming 188/255, which are no VFPU register (0–127 data, 128–143 control).
**Fix / workaround.** `tools/patches/lcs-recomp/0001` (sparse path declares both and dispatches on
`local_pc`), `0002` (out-of-range `mtv`/`mfv` emit `rt.unsupported`, like CFC1/CTC1). All 40 units
then pass `-fsyntax-only`.
**Why it isn't obvious.** The same framework builds LCS fine; the failures depend on how GT's code is
laid out.

Found: 2026-10-09 · build

### PSPRecomp's generic `psp_recomp --auto` hangs eating memory on GT

**Symptom.** No output files after 10 min, RSS climbing to the 6 GB cap; smaller unit spans don't help.
**Cause.** `tools/codegen_main.cpp`: on a `jal` whose target is an import stub, the emitter writes
`return;` and then `continue`s the per-instruction `while` **without advancing `pc`** — the same
instruction is emitted forever. Only the generic path has it; the VCS profile uses its own codegen.
**Fix / workaround.** Use lcs-recomp's framework (`break` there, plus `valid_units`). Diagnose such hangs
with `gdb -batch -ex run -ex bt` started under the cgroup (attaching with `gdb -p` is blocked here:
`ptrace_scope = 1`).
**Why it isn't obvious.** The analyzer on the same input finishes in 0.7 s; the hypothesis "big units +
libstdc++ `std::regex`" looked plausible and was wrong.

Found: 2026-10-09 · re

### Ghidra headless prints hundreds of "Error creating LSDA" lines

**Symptom.** Importing `EBOOT.elf` logs ~389 `ERROR Error creating LSDA @ 00000000 Unable to read bytes
at ram:1427bdff` with stack traces from `GccExceptionAnalyzer`; the import still reports success.
**Cause.** Not investigated in depth: the GCC exception analyzer follows LSDA pointers from `.eh_frame`
FDEs that resolve outside memory (likely an encoding/relocation the analyzer mis-reads on this PRX).
**Fix / workaround.** Ignore for now; it does not stop analysis (23,208 functions found). If exception
tables ever matter, disable "GCC Exception Handlers" in analysis options and parse `.gcc_except_table`
separately.
**Why it isn't obvious.** ERROR + stack traces look fatal and drown the useful output; grep for
`PROBE`/`REPORT` lines.

Found: 2026-10-09 · re

### REA refuses the PSP ELF: "unsupported ELF architecture"

**Symptom.** `rea analyze extracted/EBOOT.elf --provider ghidra` (and `open_binary` over MCP) fails in
~7 ms with `target_unavailable` / "unsupported ELF architecture", although `rea doctor --provider
ghidra` reports Ghidra 12.1.3 available. Patching `e_type` from `0xFFA0` to `EXEC` changes nothing.
**Cause.** REA 6.2.0 admits ELF only for `e_machine` 3/62/40/183 (x86, x86_64, ARM, ARM64) —
`dist/domain/binaryTarget.js`, `elfArchitecture()`. MIPS (8) is rejected before any provider runs.
**Fix / workaround.** Use the same engine directly: Ghidra 12.1.3 + ghidra-allegrex via
`analyzeHeadless` or the GUI (see reverse-engineering.md). REA stays useful for non-native artifacts
only. Re-check on each REA release (`grep -n -A12 "elfArchitecture = " …/binaryTarget.js`).
**Why it isn't obvious.** The error mentions architecture, not machine type; doctor says healthy; the
provider (Ghidra) supports MIPS fine.

Found: 2026-10-09 · re

### A test still fails after reverting a deliberate break

**Symptom.** Broke a test constant with `sed`, saw it fail, reverted it with `sed` — and the test kept
failing with the *old* value in the assertion message.
**Cause.** pytest caches assertion-rewritten bytecode in `__pycache__` keyed by source mtime and size;
a revert within the same second that keeps the size identical reuses the stale `.pyc`.
**Fix / workaround.** After a break-and-revert, `find tools -name __pycache__ -exec rm -rf {} +`
before rerunning (or touch the file a second later).
**Why it isn't obvious.** The file on disk is correct; `-p no:cacheprovider` does not help (that is a
different cache).

Found: 2026-10-09 · tests

### PSPRecomp seems to implement none of GT's imports (it implements 151)

**Symptom.** Searching PSPRecomp's sources for GT's 329 NIDs as 8-hex-digit literals found 0.
**Cause.** NIDs are written with an unsigned suffix (`0x42778A9Fu`), so a regex ending in `\b` after
the digits never matches.
**Fix / workaround.** Match `0[xX]([0-9A-Fa-f]{8})(?![0-9A-Fa-f])`, or better, parse the
`register_hle("<lib>", 0x…u, …)` calls (lib + NID pairs).
**Why it isn't obvious.** Zero coverage is a plausible answer for a project built for another game.

Found: 2026-10-09 · re

### "After `jr $ra`" is not where a function ends

**Symptom.** A function cut at its first `jr $ra` + delay slot had branches pointing past its end;
the "next function" was a two-instruction `return 0` tail.
**Cause.** GCC places early-return tails after the main epilogue (e.g. `func_00013DD0`'s `return 0` at
`0x13e1c`).
**Fix / workaround.** Extend a function to cover every branch target that falls after its first
`jr $ra`; use relocations and call targets (`jal`) for real starts.
**Why it isn't obvious.** For most small leaves the heuristic happens to be right.

Found: 2026-10-09 · re

### decomp.me's `psp-gcc-1.7.1` has no assembler or linker

**Symptom.** `psp/bin/as`, `ld`, `ar`, `g++` in the 1.7.1 tarball are 0-byte files; only `gcc`/`cc1`
work, so `gcc -c` fails.
**Cause.** decomp.me assembles with its own assembler; the tarball only ships what it needs.
**Fix / workaround.** `gcc -S`, then assemble with `psp-as` from the **1.3.1** tarball
(`GNU assembler 2.14+allegrex-vfpu-2.2.1-psp-1.3.1`): `psp-as -EL -G0 x.s -o x.o`.
**Why it isn't obvious.** The files exist and are executable.

Found: 2026-10-09 · build

### decomp.me compiler ids don't say which GCC they are

**Symptom.** `psp-gcc-1.3.1` / `psp-gcc-1.7.1` look like tool versions, not compiler versions.
**Cause.** They are Sony SDK `psp-gcc` release numbers; the GCC inside is 3.3.3 in both.
**Fix / workaround.** `strings <cc1> | grep allegrex`, or compile with `-S` and read `.ident`.
**Why it isn't obvious.** GT's `.comment` uses the same `psp-X.Y.Z` scheme (1.14.0, 1.15.3, 2.8.2), which
invites comparing release numbers instead of GCC versions.

Found: 2026-10-09 · build

### REA has no native engine although Ghidra is installed

**Symptom.** `binary_session` lists the Ghidra provider as `not_configured`: "Set GHIDRA_INSTALL_DIR to an
extracted Ghidra 12.1.x release directory." `open_binary` on the ELF would have no deep provider.
**Cause.** The user-scope REA registration (`npx -y rea-agents@latest mcp`) has an empty `env`; REA does
not discover `/opt/ghidra` on its own.
**Fix / workaround.** This repo's `.mcp.json` registers `rea` with `GHIDRA_INSTALL_DIR` (default
`/opt/ghidra`). Claude Code asks once to approve the project server; restart the session afterwards.
Allegrex/PRX support additionally needs the ghidra-allegrex extension — see
[toolchain](agents/toolchain.md#ghidra--ghidra-allegrex).
**Why it isn't obvious.** `ghidra` is on `PATH` and REA reports "Connected"; only the provider detail
says the engine is missing.

Found: 2026-10-09 · env

### readelf calls the Allegrex flags "gs464"

**Symptom.** `readelf -h` prints `Flags: 0x10a23001, noreorder, gs464, eabi32, mips2`.
**Cause.** Binutils maps machine value `0x00a20000` to Loongson GS464; on PSP that value is Allegrex.
**Fix / workaround.** Ignore the name. decomp.me's PSP assembler even passes `-march=gs464`.
**Why it isn't obvious.** It looks like a wrong-architecture binary.

Found: 2026-10-09 · re

### `.eh_frame` looks like a function-boundary oracle but covers 4.4 % of `.text`

**Symptom.** 774 FDEs for a binary with ~28k `jr $ra`.
**Cause.** Most of the code was compiled without exceptions; FDEs exist only for the parts built with
them (starting at `0x356960`, likely libraries / some C++ units).
**Fix / workaround.** Use splat's own function detection plus `jr $ra`/relocation evidence; treat FDEs
as exact boundaries only where they exist.
**Why it isn't obvious.** C++ binaries with `.gcc_except_table` usually have FDEs for most functions.

Found: 2026-10-09 · split

### `.comment` lists three compilers and you can't tell which built the game code from it

**Symptom.** `GCC 4.0.4 psp-2.8.2`, `GCC 3.3.6 psp-1.15.3.internal`, `GCC 3.3.6 psp-1.14.0`, once each.
**Cause.** The linker merges `.comment` (SHF_MERGE|SHF_STRINGS) and deduplicates identical strings, so
the per-object attribution is gone.
**Fix / workaround.** Attribute by test-compiling small game functions against candidate compilers —
done on 2026-10-09, see [toolchain](agents/toolchain.md#compiler--searched-not-found).
**Why it isn't obvious.** One string per compiler reads like "one object each".

Found: 2026-10-09 · build

### `BOOT.BIN` is a zero-filled decoy

**Symptom.** `PSP_GAME/SYSDIR/BOOT.BIN` has the exact size of the decrypted ELF (7,057,972 B) but every
byte is `0x00`.
**Cause.** Publishers blank the unencrypted boot image on retail discs.
**Fix / workaround.** Decrypt `EBOOT.BIN` with pspdecrypt instead.
**Why it isn't obvious.** On some PSP discs `BOOT.BIN` is the plain executable; the size match makes it
look valid.

Found: 2026-10-09 · extract

### `isoinfo -x` writes empty files for this ISO

**Symptom.** `isoinfo -i <iso> -x '/PSP_GAME/SYSDIR/EBOOT.BIN;1'` exits 0 and writes 0 bytes.
**Cause.** Not investigated (path/version-suffix handling).
**Fix / workaround.** Use `7z x` (see [FACTS](FACTS.md#commands-that-work-here)).
**Why it isn't obvious.** Exit code 0, no error message.

Found: 2026-10-09 · extract
