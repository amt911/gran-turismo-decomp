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

### A race never finishes loading: 10.5 MB free, a 6.6 MB block refused

**Symptom.** After the track's "Confirm" the GT logo stays on screen; the log shows
`sceKernelAllocMemoryBlock(PDI:ABLOCK, type 0, 0x006A0000) failed: … 1141 free ranges, 10500864 bytes
free, largest 196608`.
**Cause.** Our memory-block allocator bump-allocated from the bottom and never merged freed ranges.
The firmware's PSP_SMEM_High (type 1) — which GT uses for ~96 % of its blocks (PPSSPP log) — takes
from the top, keeping long-lived and transient blocks apart.
**Fix / workaround.** `host/gt_kernel.cpp`: sorted, merged free list; type 1 from the top (sharing
`next_stack_top` with thread stacks), type 0 from the bottom; a freed range touching a cursor
returns to it. The race loads and runs.
**Why it isn't obvious.** Plenty of memory was free; only the largest-piece figure tells.

Found: 2026-10-10 · runtime

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

### Menu icons and the race HUD missing: sprites drawn before the triangles under them

**Symptom.** Single Player's mode panels empty (no helmet/car/track icons, no arrows, no OK
button), the main menu's "i" button and most of the race HUD (position, times, speed, gauges)
missing — while the frame-by-frame trace showed those sprites being drawn, with the right colours.
**Cause.** lcs-recomp's software renderer defers triangles (batched until the list ends or a
transfer) but rasterizes sprites (PRIM 6) at once, so a sprite lands *under* panels the list drew
before it. LCS rarely mixes the two; GT's UI does it everywhere.
**Fix / workaround.** Patch 0015: before a sprite, drain the HW draw queue and flush the deferred
triangles whose rows it overlaps. Costs ~3 % of speed.
**Why it isn't obvious.** Every individual draw is correct (texture, CLUT, blend); only the order is
wrong. A PPSSPP reference settled it: PPSSPPHeadless's websocket debugger never answered here, but
PPSSPPSDL under `xvfb-run`, driven with python-xlib's XTEST key presses (Space = START, Z = Cross)
and `import -window root` screenshots, gives the real frames.

Found: 2026-10-10 · renderer

### "Save data created on another system": the savedata `bind` field

**Symptom.** Loading any save GT wrote showed "Save data created on another system has been
detected…" (earlier, with another HLE state, a white screen).
**Cause.** After a secure load GT's loader (0x08A1CECC, called by `MSaveDataUtilPSP.autoLoad`)
returns 0xE unless `(param->bind & 3) == 1` — `bind` (offset 0x34 of SceUtilitySavedataParam) is
what the firmware sets to say the save belongs to this console. The HLE never wrote it.
**Fix / workaround.** `load_savedata_file` sets `bind = 1021` (as PPSSPP); the `sfoParam` is saved
next to the data (SFOPARAM.BIN) and given back on load like PARAM.SFO.
**Why it isn't obvious.** The MAC address (`sceWlanGetEtherAddr`) is read on that path and looks
like the identity check; it isn't — no import is called between the load and the refusal.

Found: 2026-10-10 · savedata

### GT's sound never reached the output: sceAudioOutput2 and GT.VOL music

**Symptom.** No audio at all (no `sceAudioOutput*` channel ever written), and the Atrac music
silent.
**Cause.** GT mixes everything (SAS) into `sceAudioOutput2OutputBlocking`, whose HLE only slept;
lcs-recomp's Atrac HLE decodes from a loose file found by header match on the disc — GT's music sits
inside GT.VOL, so no file matched and the context produced silence.
**Fix / workaround.** Output2 buffers go to `audio_output_submit` (channel 7) and `GT_AUDIO_DUMP`;
Atrac contexts without a file keep the bytes the game streams into its buffer (load +
`AddStreamData`, by file offset) and decode them frame by frame with FFmpeg's atrac3p decoder
(`host/atrac_decoder.cpp`). Menu RMS went from ~700 (effects only) to ~5,800.
**Why it isn't obvious.** Everything counted and timed correctly — samples, frames left, loops — so
the game behaved; only the output was empty.

Found: 2026-10-10 · audio

### Decompiled functions called only from their own generated unit never ran

**Symptom.** The page-heap helpers (`find_page`, `size_class`, …) showed `calls 0` in a race-path
diff although memalign and free call them about a million times.
**Cause.** psp_recomp lowers a fixed `jal`/`j`/branch whose target lies in the same generated unit to
a C++ `goto` to that label; only import stubs were sent back to the dispatcher. A decompiled function
replaces the original in the dispatcher's table, so every same-unit caller kept running the
recompiled original — in `--decomp on` too. Functions with callers in other units (most natives)
were unaffected, which hid it.
**Fix / workaround.** Patch 0014 (`codegen-extra-boundaries`) adds an optional boundaries file to
`psp_recomp --auto`; its addresses are treated like import stubs, before the same-unit label check.
`tools.recomp` writes the active `GT_DECOMP` addresses there and includes them in its input digest.
**Why it isn't obvious.** The gate stays green — it just never sees those calls; only the `calls`
column (or a sabotage that changes nothing) gives it away.

Found: 2026-10-10 · recomp

### A hot function's rare branch can pass the gate unexercised — even with every case changing memory

**Symptom.** `free` (1.1 million calls to the race) passed 28/28 with a sabotage in its page-relink
branch (taken when a full page gets its first free slot) — and all 28 cases had changed memory, so
the column above looked healthy.
**Cause.** Geometric sampling picks ~30 calls out of a million; a branch taken in a few percent of
calls is easily missed. Re-reading the asm then found a real bug the samples had missed too (the
page-release call lacked its class/page arguments).
**Fix / workaround.** Run hot functions with rare branches dense as well, with a big budget:
`GT_DECOMP_DENSE=0x08B35254 build/gt_native --max-seconds 120 --decomp diff --decomp-cases 2000`
caught the sabotage (7 of 2,000 cases). Sabotage the rare branch on purpose to prove it is covered.
**Why it isn't obvious.** Nothing in the report tells "branch exercised" from "branch never
reached"; only a targeted sabotage does.

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

### Import stubs look like empty functions

**Symptom.** Registering every `jr $ra; nop` function as `empty_function` gave mismatches (RAM,
kernel state) in 9 of them, all at `0x08C7A7F8`–`0x08C7B240`.
**Cause.** That range is `.sceStub.text`: the import stubs are `jr $ra; nop` in the file and the
loader patches them into syscalls. In the ELF they look empty, but at runtime they are not.
**Fix / workaround.** Templates found by scanning code shapes skip `.sceStub.text`.
**Why it isn't obvious.** Several of the stubs compared *equal* (calls with no visible effect), so
the stubs that passed would have been registered as empty functions too.

Found: 2026-10-10 · src/core

### A registration without a comma was invisible to progress and boundaries

**Symptom.** `tools.progress` counted 892 of 943 registrations; `decomp_boundaries.txt` had 896.
**Cause.** The registration regex required a `,` after the address. One-argument templates
(`GT_DECOMP_EMPTY(0x…u);`, `GT_DECOMP_ZERO`) end in `)`.
**Fix / workaround.** The regex accepts `,` or `)`. Re-run `tools.recomp` after the fix.
**Why it isn't obvious.** Without a boundary, a function is still compared when it is called from
another unit, so the diff run looked complete.

Found: 2026-10-10 · tools

### A local constant can silently shadow one from `value.hpp`

**Symptom.** None. The build was clean and the diff stayed green, but an existing native
(line ~742 of `game_natives.cpp`) began calling virtual slot 15 instead of 12.
**Cause.** A new `constexpr kToBoolSlot = 15` in the file's anonymous namespace shadowed
`gt::adhoc::kToBoolSlot = 12` from `value.hpp`. Slot 15 is `toInt`, not `toBool`. The function's
cases happened to return the same value either way.
**Fix / workaround.** Before declaring a constant, grep `src/` and `value.hpp` for the name *and* the
value (reuse first). The slots are `kToBoolSlot` 12, `kToIntSlot` 15, `kToStringSlot` 3, and so on.
**Why it isn't obvious.** C++ allows the shadowing with no warning, and a green gate does not prove
the right slot is called when both slots give the same answer.

Found: 2026-10-10 · src/game/adhoc

### A template name with a digit was invisible

**Symptom.** `GT_DECOMP_RETURN_ARG0(...)` registrations were not counted by `tools.progress`, were
missing from `decomp_boundaries.txt`, and were not filtered by the candidate scripts. All 30
candidates stayed registered, including the ones never compared.
**Cause.** Every registration regex used `GT_DECOMP(_[A-Z_]+)?`, which excludes digits.
**Fix / workaround.** The progress regex accepts `[A-Z0-9_]`, with a test. The macros were renamed to
`GT_DECOMP_RETURN_FIRST_ARG` / `_SECOND_ARG`.
**Why it isn't obvious.** The build compiles and the diff stays green: registration works through the
C macro, and only the tools that read the source with a regex miss it.

Found: 2026-10-11 · tools

### A diff run that stopped early still exited 0

**Symptom.** Every `--decomp diff` run after PR #50 printed
`Runtime stopped: call_guest(0x0893244C): no function at 0x0893244C or no return` and exited **0**.
Every function compared before the stop showed `mismatched 0`, so the gate looked green. The race
route stopped at car selection and never reached the race.
**Cause.** A typo in a decompiled native's callee (`kCarSpecDestroy` 0x0893244C instead of
0x08932448, the `jal 0x12e448` target). `call_guest` stops the runtime when the target isn't a
function, and `gt_native` returned only the harness's status.
**Fix / workaround.** The address is fixed. `gt_native` now exits **3** when the runtime stops for
anything other than the wall-clock bound or "All PSP threads completed"; a mismatch (2) still wins.
After every diff run, check the exit code and the `Runtime stopped:` line, not only the
`mismatched` counts.
**Why it isn't obvious.** The comparisons made before the stop are all real and equal, and nothing
says the run covered far less than usual. The only hint is that functions later in the route show
`calls 0`, which looks like flaky coverage.

Found: 2026-10-11 · host/main.cpp, src/game/adhoc

### The whole game ran at half speed (two vblank waiters, paced per call)

**Symptom.** In `gt_play` the game, its music and its effects were slow. `GT_PERF_LOG=1` showed
~60 `sceDisplayWaitVblankStart` calls per second, but guest time advancing at **0.50×** real time and
the audio mix arriving at ~22,050 frames/s instead of 44,100.
**Cause.** Two GT threads wait for every vblank. The host slept one real vblank period (16.7 ms) on
each **call**, so each guest vblank cost two real periods. `gt_native` had the same pacing; frame dumps
looked normal because they count presented images, not time.
**Fix / workaround.** `throttle_vblank_to_real_time` now ties guest time to the wall clock. The vblank
a caller waits for is due at `anchor + (its guest time − anchor's guest time)`. The anchor resets
after a stall longer than 6 vblanks. Result: guest time 1.00× and audio 44.1 kHz. `GT_PAD`'s vblank
counts are calls, so 120 is about 1 s now, and the pad routes reach the same states in half the wall
time.
**Why it isn't obvious.** Every per-call counter looked right: 60 vblanks/s, no skips, 85 % idle.
Only guest time per wall second showed it.

Found: 2026-10-11 · host/gt_kernel.cpp

### Idle rasterizer threads burned half the CPU

**Symptom.** `gt_play` used about 4.4 cores. On a power-limited laptop (Ryzen 7 5850U) it ran slower
than the work required.
**Cause.** lcs-recomp's software-rasterizer row workers spin 262,144 `pause`s before they park. GT
submits many small parallel jobs, so the 7 workers almost never parked. gprofng put about 60 % of all
CPU time in the worker loop itself.
**Fix / workaround.** Patch 0017 makes the spin configurable (`PSPRECOMP_RASTER_SPINS`), and
`host/main.cpp` defaults it to 2,048. Over 120 s of the race route, CPU went from 528 s to about
240 s with the same GE wait. On 4 cores / 8 threads, vblank skips went from 8.0/s to 3.5/s. The same
patch makes the window show 60 fps instead of a fixed 30 (`Timing.FrameRate`).
**Why it isn't obvious.** The time shows up as "work" in `top`, not as idle or waiting. A big desktop
CPU hides it completely, because spare cores absorb the spinning.

Found: 2026-10-11 · tools/patches/lcs-recomp

### `tools/setup.sh` refused its own patched tree

**Symptom.** A second `tools/setup.sh` run failed with
`patch does not apply: …/0002-codegen-trap-nonexistent-vfpu-register.patch`.
**Cause.** Each patch was tested alone with `git apply --reverse --check`. Once a later patch touches
the same lines, the reverse check of the earlier one fails.
**Fix / workaround.** The series is applied as a whole onto a clean checkout of the pinned tree. The
series hash is kept in `tools/src/lcs-recomp/.gt-patches`, and a run whose hash matches skips the step.

Found: 2026-10-11 · tools/setup.sh

### On a modest CPU the menus and sound still slowed down: vblank catch-up vs. a frame-locked game

**Symptom.** After the real-time pacing fix, a Ryzen 7 5850U laptop in "balanced" mode showed ~40 fps,
but the menus and the sound still ran slow. In "performance" mode they did not. On 2 cores / 4 threads
(`taskset`), `GT_PERF_LOG` showed 106 vblank waits/s instead of 120, audio at 36.7 kHz, 16 skipped
vblanks/s, and the guest blocked on the GE ~58 % of the time.
**Cause.** When the host fell one vblank behind, the pacing skipped vblanks to keep guest time on the
clock. GT advances its menus, its animation and its SAS mix per processed frame, so every skipped vblank
is game time that never runs, and it shows as slowness even though `guest time` reads 1.00×. The
lateness itself came from the software rasterizer: `sceGeListEnQueue` waits for the previous list.
Skipping every screen draw cut the CPU time from ~400 s to 45 s over a 120 s route.
**Fix / workaround.** A lag of up to 4 vblanks is caught up by not sleeping instead of by skipping. In
`gt_play` a frameskip (`GT_FRAMESKIP`, patch 0018) stops rasterizing draws into the two screen
framebuffers while the host is more than 1 ms late, for at most 4 waits in a row. Render-to-texture
draws are kept and vertices still advance the list state. The window keeps the last complete image.
On 2 cores this gives 121.8 waits/s, 44.0 kHz audio and 0.08 skipped vblanks/s.
**Why it isn't obvious.** `guest time 1.00x` looks like full speed. The number that matters for GT is
the vblank waits per second, about 120.

Found: 2026-10-11 · host/gt_kernel.cpp, tools/patches/lcs-recomp

### The diff gate is slow because of the harness, not the game

**Symptom.** Each `--decomp diff` route took its whole wall time (race 600 s), and `LCS_UNCAPPED=1`
(no real-time pacing) barely helped.
**Cause.** With every decompiled function installed, the harness snapshots and restores 32 MiB of RAM
several times per compared case, about 25,000 cases on the race route. The game then runs at ~0.7×
real time on average, so there is no pacing left for `LCS_UNCAPPED` to remove.
**Fix / workaround.** `GT_DECOMP_ONLY=0xADDR,...` installs only the listed functions. With one batch
installed the game runs at 1.0× capped and 1.4× uncapped. A 200 s uncapped race compares the same
batch functions with the same case count as the full 600 s run. Iterate on a batch with that, and run
the full gate once before the PR (`docs/agents/decomp-workflow.md` § Working in batches).
**Why it isn't obvious.** Each case looks cheap. The cost only shows as the guest-time factor in
`GT_PERF_LOG`, which never reaches 1.0× in a full diff run.

Found: 2026-10-12 · host/diff_harness.cpp

**Update 2026-10-14 — fixed.** With ~4,900 functions installed the full gate had become far worse than 0.7×:
the menu routes advanced 1–5 vblanks a second (120 at real time), so in 150–300 s dealer, garage, buy and
options never left the title screen and compared only boot functions — the full gate was no regression check
for the menus. The harness now write-protects guest RAM for each case (`host/dirty_pages.hpp`): the first
write to a page faults, the handler saves the page and lets the write through, and the case copies, restores
and compares only the pages either run wrote (`gt::first_page_difference`, same message as the whole-RAM
compare). The buy route in diff mode with every function: 1–5 → ~120 vblanks/s, 1,258 → 2,933 functions
compared in 90 s, 0 mismatched. Checks that it still sees what it must: the batch-63 sabotage (23/23
registered functions detected), a decompiled run that skips a store the original makes ("RAM at 0x09FCFBD8 …
4 bytes differ") and one that writes a page the original doesn't touch ("RAM at 0x08000F80").
A first version also saved every page before each import a case runs (an HLE writing guest RAM through a
system call would get EFAULT on a protected page instead of a fault); nearly every case calls a timer or
lock import, so that copied 32 MiB per case again and gained nothing. The imports allowed in a case write
through `GuestMemory`, so they fault like any store; `host/main.cpp` says so next to the lists.
The first full gate at real time compared 4,364 of 4,882 functions (2,474 before) and found a bug the old
gate never reached: `func_08A15D54` (dealer and options menus) called `func_08A15800` without the `a3 = 0` its
delay slot sets, and that callee reads `a3 & 0xFF` — fixed in the same change.

### A callee that leaves v0 alone returns the caller's v0

**Symptom.** A decompiled function mismatches only in v0. The original returns a value its own code
never computes: -1 in `func_0892FBE0`, `index << 6` in `func_08AD1A3C`, `this` in `func_08A77FF8`.
**Cause.** Some callees are void and never write v0, so the original returns whatever v0 held when
it made the call. That is often a temporary the compiler left there, such as a constant loaded for a
store or an address computation. `call_guest` starts the callee from the decompiled caller's context,
so the callee inherits a different v0.
**Fix / workaround.** Before the call, set `ctx.gpr[2]` to the value the original has in v0 at that
point, read from the generated code, and comment why. Then use the call's result as the return value.
The same applies to v1 and to a tail call's target.
**Why it isn't obvious.** The C++ looks complete. The leftover value only shows up in the
register-level diff, and only on the paths where the callee skips writing v0.

Found: 2026-10-12 · src/game/unsorted/hot_leaves.cpp

### A string property's local must start as the empty string

**Symptom.** Decompiled readers of string properties fault inside the visitor (`Guest memory access
outside PSP RAM/EDRAM` at 0xFFFFFFFF or a small address), on every case.
**Cause.** Property readers hand the visitor a stack descriptor `{vtable, empty name, -1, &value}`. For an
int or float the value slot can start uninitialised, because the visitor only stores into it. For a string
(vtable 0x08CCD9D8) the visitor *assigns* to it, which releases the old representation first, so the slot
must hold the empty string (0x08D74FDC) beforehand. The original sets it up before the call.
**Fix / workaround.** Read string properties with `visit_string`, which initialises the slot. Don't use
`read_property` for strings.
**Why it isn't obvious.** All property reads share one descriptor layout. The generic helper works for
every numeric type, and the extra store before the call looks like a register-allocation leftover.

Found: 2026-10-12 · src/game/unsorted/hot_leaves.cpp

### An XOR sabotage cancels in a function that calls another sabotaged one

**Symptom.** The batch sabotage reported one function undetected (`func_08A635E0`, 11 cases, all equal)
although its decompiled body is plain C++ like the rest.
**Cause.** The sabotage flipped s0 with `ctx.gpr[16] ^= 1u` at the entry of every function of the batch.
`func_08A635E0` is a thin wrapper that calls `func_08A63714` directly as C++, and that function was in the
same batch, so s0 was flipped twice and came back unchanged.
**Fix / workaround.** Bump instead of flip (`ctx.gpr[16] += 1u`): nested sabotaged calls add up and never
cancel. With that, `func_08A635E0` mismatched in 11/11 cases.
**Why it isn't obvious.** Each sabotage looks independent. The cancellation needs one function of the
batch to run another one's sabotaged body, as a thin wrapper does.

Found: 2026-10-12 · src/game/unsorted/hot_leaves.cpp

### A callee can read a stale stack slot that holds the original caller's saved s-register

**Symptom.** `func_0894D484` (place car i on the grid) mismatched in every case on one byte of the car's
state: the original wrote `FC` there in every case, the decompiled version a different byte each time.
This held even after fixing a real bug, a leftover `a1` from `func_0894CF40`.
**Cause (inferred, not traced).** The original keeps its `pos` argument (a stack address `0x09FCxxxx`)
in s1. The deep callee chain `func_08941510` → `func_0894106C` → `func_0893C3E4` copies locals that are
never written on the path taken. Those locals still hold whatever earlier frames saved there, which in
the original includes that s1. The decompiled caller keeps `pos` in C++, so the saved s1 at that stack
address is the harness caller's value, and it differs from run to run.
**Fix / workaround.** None that stays readable: we would have to reproduce the original's register
allocation. The function was dropped from batch 53 and added to the candidate skip list.
**Why it isn't obvious.** The decompiled function's own logic is correct. The divergence comes from
uninitialised stack two calls down. "Dead stack is ignored" holds for comparison only: a callee can
still read it and turn it into live memory.

Found: 2026-10-12 · src/game/unsorted/hot_leaves.cpp

### What the Vulkan GE backend draws wrong on GT (phase 1 measurement)

**Symptom.** With `NativeGE=true` the GPU gate
(`tools/gpu_gate.sh`, ×1, pixel equality against the software rasterizer in the same run) passes only
61/841 race frames, 47/607 dealer frames and 62/637 garage frames.
**Cause.** Four distinct problems, read from the worst frames' difference images and `GT_GPU_STATS`:
1. **The 3D scene is not drawn.** In the race the track, sky and cars are black and only the HUD shows;
   in the dealer the car model is missing. The 2D/through-mode draws work.
2. **CPU-updated textures are stale.** Dealer text comes out as wrong glyphs ("1 1 1…"): GT rewrites
   its font atlas in RAM between frames and the backend keeps the old upload.
3. **Screens composed through a render target are black.** The whole garage page is black on the GPU.
   The backend never writes its pixels back to guest memory (spec § write-back).
4. **Three blend states are unsupported** and draw opaque. GT only uses the add equation. `src 2·α /
   dst 1−2·α` (factors 6/7) is ~40 M draws in a 600 s race, mainly HUD and effects; it makes the
   unlit digital-speed segments show. `fix 0x808080 / fix 0xFFFFFF` and `fix 0xFFFFFF / 1−α` are rare.
**Fix / workaround.** Phase 2 (3D geometry, texture invalidation, blending) and phase 3 (write-back) of
`docs/superpowers/specs/2026-10-12-gpu-ge-backend-design.md`. Until then `gt_play` keeps the software
rasterizer (`NativeGE=false`).
**Why it isn't obvious.** The backend renders LCS fine, and GT's menus look right at a glance. Only the
paired comparison shows that the 3D path and the dynamic textures are missing.

Found: 2026-10-12 · tools/src/lcs-recomp/lcs/host/ge_gpu_backend_vulkan.cpp, tools/gpu_gate.sh

### Vulkan never writes depth while its depth test is off; the GE does

**Symptom.** With the GPU GE backend, the dealer's 3D car was missing entirely. Turning the depth test
off made it appear (in the wrong order), and resetting depth to far every frame drew it correctly.
**Cause.** GT clears its z-buffer with clear-mode draws: full screen, z = 65535, depth *write* on, depth
*test* off. In Vulkan, `depthWriteEnable` has no effect unless `depthTestEnable` is true, so those clears
never reached the depth image, and the car's `LEQUAL` pass failed against stale depth. Two smaller
mismatches made it worse. The PSP has one z-buffer for both framebuffers, while the backend had one depth
image per target. The backend also cleared depth on its own at the start of each frame.
**Fix / workaround.** lcs-recomp patch 0021: draws that write depth without testing it use the test with
`ALWAYS`. One depth image is shared by every target, and depth is never cleared outside the game's own
draws.
**Why it isn't obvious.** The draw state looks right in every log (write on, test off), and the same
state works on the software rasterizer and on OpenGL-style APIs.

Found: 2026-10-12 · tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch

### The GPU backend hands back the previous frame, so a naive gate compares N with N−1

**Symptom.** In the GPU gate, the dealer's turntable car looked shifted or rotated against the software
frame, and the scrolling news line was offset by a few pixels. Static screens matched.
**Cause.** `ge_gpu_backend_finish_color_frame` first completes the previous submit, copying its readback
into `frame_rgba`, and then submits the current frame. The image it returns at present N is frame N−1.
The software rasterizer's framebuffer at present N is frame N.
**Fix / workaround.** `gpu_frame_dump` (host/frame_dump.cpp) runs on every present and pairs the GPU
image with the previous present's software frame. That raised the race from 257 to 299 equal frames out
of 307. With a window, the backend presents directly, so play is not one frame late.
**Why it isn't obvious.** Most of GT's screens are static, so the lag only shows on moving ones, and
there it looks like a transform or skinning bug.

Found: 2026-10-12 · host/frame_dump.cpp

### A render target sampled as a texture must keep a fixed guest-pixel space

**Symptom.** On the GPU, GT's menus came out magnified and cropped (about ×2 vertically), the garage page
was black, and the 3D scene was hidden behind a black layer with only the HUD visible.
**Cause.** GT draws its HUD and 2D layer into an offscreen buffer at 0x0CC000 (stride 512). It then draws
that buffer over the scene as a 512×512 texture with alpha blending, and samples part of it as a 128×128
texture for the car reflection. lcs-recomp's backend gave every target one image at screen size and
re-mapped it to each draw's stride and scissor, which only works for a game that draws to the screen.
Its shader also quantized colour and alpha to the framebuffer format before blending. A 5551 target
therefore blended with a 1-bit alpha, while reading the target back saw 8-bit alpha instead of the
packed value the PSP stores. The result was the alpha applied twice, about ×0.69 darker.
**Fix / workaround.** lcs-recomp patch 0021 makes these changes:
- Every target covers a fixed 512×512 guest-pixel space at the internal scale, and the display shows its
  480×272 crop.
- Sampling a target scales the UVs to the texture's size.
- The shader blends unquantized values.
- Reading a target back emulates the 16-bit pack and unpack (`storedFramebufferTexel`).
**Why it isn't obvious.** LCS, the game the backend was written for, never samples its render targets,
so none of this was exercised.

Found: 2026-10-12 · tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch

### GT copies every glyph into the same texture right before drawing it

**Symptom.** On the GPU, text came out as one repeated glyph ("1 1 1…" in the dealer, "NNNN" in the
garage).
**Cause.** GT uses a block transfer (6×1 to 6×n pixels into 0x08CF6930) to copy a glyph into a small
texture, draws it, then copies the next glyph to the same address. The backend computed texture
signatures once per frame and looked textures up by address when it executed the batches at the end of
the frame. Every glyph draw therefore used the last uploaded content.
**Fix / workaround.** lcs-recomp patch 0021:
- Textures are keyed by content once their signature is known (`ge_gpu_backend_finalize_texture_key`).
- `gt_ge_exec.cpp` reports the bytes a block transfer rewrote (`ge_gpu_backend_note_guest_write`), so
  the next draw re-checks them.
- Textures of 32 K texels or fewer are re-checked on every draw.
**Why it isn't obvious.** Per-frame texture caching works for nearly every game. The transfers are tiny,
and the counts only show up in `GT_GPU_STATS`.

Found: 2026-10-12 · host/gt_ge_exec.cpp, tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch

### A FIFO present on the GE thread halves GT's speed

**Symptom.** With the GPU GE backend in a real window, the race ran at about 52 vblank waits/s (120
expected), with 38 skipped per second, audio at 17 kHz and `present 28 ms avg` in `GT_PERF_LOG`. Turning VSync
off brought it to 120/s at once.
**Cause.** The backend presents from the thread that executes the GE lists, so the game waits on it.
VSync selected `VK_PRESENT_MODE_FIFO_KHR`, which blocks every present until the monitor's vblank. In
headless runs the CPU readback plus SDL's software present cost a similar stall, about 30 waits/s.
**Fix / workaround.** lcs-recomp patch 0021 uses MAILBOX whenever the driver offers it, with VSync on or
off: no tearing and no blocking. FIFO remains the last resort. Race at ×2 and ×4: 120 waits/s, 0 skipped,
44.1 kHz.
**Why it isn't obvious.** The CPU profile shows idle threads, with the game asleep in its vblank pacing
and the GE worker waiting for work, so nothing looks busy. Only the per-present time gives the stall away.
Measure performance in a window, not headless.

Found: 2026-10-12 · tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch


### Without MAILBOX, a FIFO present still stalls the GE thread

**Symptom.** On X11 with NVIDIA (FIFO and IMMEDIATE only) the stall above came back even after the
MAILBOX fix. SDL2 picks x11 unless told otherwise.
**Cause.** `vkAcquireNextImageKHR` waits up to 1 s for a free image. Under FIFO no image is free until
the monitor's next vblank.
**Fix / workaround.** lcs-recomp patch 0021: under FIFO the acquire does not wait. When no image is
free, that frame is not shown (`present_skipped_no_image`) and the game carries on. It looks like
MAILBOX, without tearing, on any platform where only FIFO is guaranteed (Android, X11, older Mesa).
**Why it isn't obvious.** FIFO is the one mode Vulkan guarantees, so it looks like the safe default.

Found: 2026-10-13 · tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch

### GT waits for vblank from two threads: presenting on every wait made the pacing uneven

**Symptom.** With `GT_PRESENT_TRACE=1`, the guest time between new frames had 6 % of intervals
under 8 ms, and the host time was spread from 8 to 30 ms. The game ran at 60 fps, but it looked jerky.
**Cause.** The HLE treated each `sceDisplayWaitVblankStart*` call as a vblank and presented on each
one: 120 presents/s for 60 fps. A flip between the two threads' waits reached the window a few ms
after the previous frame. The present was also queued when the game thread *reached* the wait, which
varies with each frame's CPU work, not at the vblank instant. With the GPU, a present without a flip
also closed a half-drawn frame.
**Fix / workaround.** `host/gt_kernel.cpp`:
- The window is updated once per guest vblank period (`virtual_time_us / kVblankPeriodUs`), like the
  PSP latching its frame buffer.
- The present runs after the real-time vblank sleep, not before it.
- With the GPU, a frame is closed only after `sceDisplaySetFrameBuf` (`g_display_flips`).

Measured result: guest intervals are 100 % in 15–18 ms, and host intervals 96 % (was 68 %).
**Why it isn't obvious.** Guest time ran at 1.00×, with 60 frames/s and 0 skipped, so every counter
was fine. Only a histogram of the intervals shows it.

Found: 2026-10-13 · host/gt_kernel.cpp

### The intro movie's sound never played: GT streams PSMF from memory

**Symptom.** The intro video played without sound.
**Cause.** `sceMpegAtracDecode` decoded audio only from the movie's file (`source_path`). GT reads its
movies out of GT.VOL and feeds them through the sceMpeg ring buffer, so there is no file and the HLE
wrote silence. The video worked because `MovieDecoder` already decoded from the ring buffer.
**Fix / workaround.** The same ring-buffer packets are demuxed for private stream 1 (`demux_audio_packet`,
dropping PSMF's 4-byte substream header). `Atrac3pFramer` then splits the ATRAC3plus frames (sync `0F D0`,
8-byte header, size measured between the first two syncs), and `AtracFrameDecoder` decodes one frame
(2048 stereo samples) per `sceMpegAtracDecode`.
**Why it isn't obvious.** The HLE comes from LCS, whose movies are plain files, and it returns success
with zeroed PCM, so nothing logs an error.

Found: 2026-10-13 · host/ps_demux.cpp, host/gt_kernel.cpp

### Two host-side costs dominated the CPU, not the game

**Symptom.** The game ran at full speed on the GPU, but it used far more CPU than its recompiled code
needs. That matters for phones and small boards.
**Cause.** Measured with gperftools (`LD_PRELOAD=libprofiler.so CPUPROFILE=...`, parsed with nm
because pprof is not installed):
- 35 % went to the rasterizer's draw-preparation workers spinning idle (`HwDrawQueue`, 2000 spins
  per wakeup, 4 threads on a 24-core box).
- 23 % went to `ge_gpu_backend_note_guest_write`, which walked every cached texture under a mutex on
  each block transfer. GT copies glyphs one transfer at a time, and the content-keyed cache holds
  thousands of textures.
- The recompiled game itself was about 15 %.

**Fix / workaround.** lcs-recomp patch 0021:
- No worker threads by default (`PSPRECOMP_GE_DECODE_THREADS` still opts in). The GE thread alone
  measured the same GE wait.
- Guest writes stamp 64 KiB pages with a running clock. A texture is re-checked when one of its pages
  carries a newer stamp, at O(pages) per write.
- Uploaded textures drop their CPU copy.

For 60 s of a race: 17.9 → 7.6 CPU-seconds, and GE wait 43 % → 7 %.
**Why it isn't obvious.** `GT_PERF_LOG` showed "GE wait" but not why. Both costs are host
bookkeeping that grows with core count and cache size, not with what the game draws.

Found: 2026-10-13 · tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch

### The GPU gate is flaky in the dealer route's intro (frames ~1320–1560)

**Symptom.** The same build passes the dealer route 100 % in one run and fails 3–12 frames between
1320 and 1560 in the next. `main` behaves the same: 106/106 in one run, 6 failures in the next.
**Cause.** Not fully known yet. Ruled out:
- The pairing. `gpu_frame_dump` assumed a fixed one-frame readback lag; each GPU image now carries
  the vblank it was closed at (`game_frame_rgba_vblank`).
- A stale texture or lag. With `GT_FRAME_EVERY=2`, every failing GPU frame matches its *own* software
  frame best, not a neighbour.
- Reporting the MPEG HLE's picture writes (`ge_gpu_backend_note_guest_write`), and folding the write
  stamp into the texture key. Neither changed the failures; the second added new ones at 2220–2400 and
  was reverted.
- The palette. The GE copies its CLUT at the load command (`0xC4`), while this emulation reads it at
  draw time and the software rasterizer only at its deferred flush. A temporary checksum instrument
  found that 0 of 32142 CLUT loads in the dealer route changed before the flush (2026-10-13).

What is left: a real rendering difference that depends on timing. The worst frames show purple light
where the software rasterizer shows white, which suggests a CLUT or a render-to-texture read at a
different moment.
**Fix / workaround.** None yet. Compare the gate's dealer counts across two runs before calling a
change a regression.
**Why it isn't obvious.** One run per build looks like a regression or a fix. Only repeated runs, and
the same check on `main`, show it is noise.

Found: 2026-10-13 · host/frame_dump.cpp, tools/gpu_gate.sh

### "Pixelated" GPU output: the internal resolution did not follow the window

**Symptom.** With `NativeGE=true` the picture still looked pixelated after maximizing or going
fullscreen.
**Cause.** The internal resolution was fixed at start: a scale, or the desktop size. A window
enlarged later was upscaled from that picture.
**Fix / workaround.** lcs-recomp patch 0021 adds `InternalResolutionMode=Window`, now the default in
`config/gt_play.ini`. The PSP picture is rendered at the smallest whole scale that covers the window's
drawable pixels (`uniform_internal_resolution`), with `MaxInternalScale` as a cap for weak GPUs. Between
frames, when the window changes size, the render targets are rebuilt at the new size. The terminal
prints `[vulkan] internal resolution WxH` on each change.
**Why it isn't obvious.** A capture of a 960×544 window looks sharp. Only a resize shows the problem.

Found: 2026-10-13 · tools/patches/lcs-recomp/0021-gpu-ge-for-gt.patch, config/gt_play.ini

### Leaving a harness case clears the "kernel refused" flag

**Symptom.** With import record & replay, every route stopped in boot: "No recompiled function
registered at 0x08D70000". The trace showed cases continuing live from game code right after an LwMutex
lock, not from the lock's stub.
**Cause.** `set_kernel_isolated(false)` (the `InCase` destructor) also resets
`kernel_isolation_broken()`. The harness read the flag after leaving the case, saw "not refused", and
resumed from the point after the refused call. The game then believed it held a mutex it never got.
**Fix / workaround.** `host/diff_harness.cpp` reads the flag inside the case (`original_refused`). The
continuation uses the registers saved before the refused import.
**Why it isn't obvious.** The flag's comment says "since set_kernel_isolated(true)". Until then no code
needed it after the case ended.

Found: 2026-10-13 · host/kernel_isolation.cpp, host/diff_harness.cpp

### One shared continuation slot silenced the whole harness

**Symptom.** The first full gate with import replay had 0 mismatches, but 3869 functions were never
compared, the same number on all three routes (1683 before).
**Cause.** A case that continues live waits for the original to return to its sentinel. The
first version had a single slot and started no new case while it was pending. One sampled thread loop
never returns, so from then on no case started anywhere.
**Fix / workaround.** Each case takes its own sentinel address from a pool of 1024, and the handler
frees it when the original returns there. The compared volume went back to normal (24141 cases in the
race, 24881 before).
**Why it isn't obvious.** "0 mismatches" looks like a pass. Only the never-compared count shows that
nothing was compared.

Found: 2026-10-13 · host/diff_harness.cpp

## An s0 sabotage is invisible in a function reached only nested

**Symptom.** With nested callees running decompiled, a sabotage (s0 += 1 at entry) of
`race_operator::set_organizer` went undetected, although the function ran inside an equal case.
**Cause.** s0 is callee-saved: the decompiled caller saves it on entry and restores it before it returns,
erasing the bump, and the saved copy sits in the stack window below the case's entry sp, which the
comparison ignores. A top-level case sees the bump because no caller restores it inside the case.
**Fix / workaround.** `tools.decomp_gate sabotage` also bumps a guest word in the kernel region
(`SABOTAGE_WORD`, 0x08000F00): the whole RAM is compared and GT's user-mode code never touches it, so the
bump survives any caller.
**Why it isn't obvious.** The function did run (nested in 1 equal case) — the sabotage was there and the
gate stayed green.

Found: 2026-10-13 · tools/decomp_gate.py

## A "Void" registration that loses v0 passes the gate

**Symptom.** With every decompiled callee running decompiled inside its callers' cases, the full gate
reported 1131 mismatching functions that all passed on their own.
**Cause.** Return contracts. `func_08987A74` was registered `Void` and its decompiled version left v0 as
the caller had it; the original returns the value it stores, and `func_08987A9C` reads it as the token.
A top-level case doesn't compare v0 of a `Void` function, so the bug only shows through a caller. Parents
that pass a `Void` callee's leftover v0 on as their own result mismatch the same way.
**Fix / workaround.** `func_08987A74` sets v0 and is registered `Word`. Nested execution is opt-in per
callee (`GT_DECOMP_NESTED`) until the generic registrations' contracts are audited.
`tools.contract_audit` then listed 80 registrations whose callers read an uncompared return register.
Registering each with the contract its callers need and gating it: 49 already set the register right
(now `Word`/`DoubleWord`, so the gate checks it); 30 really lose it — under `--decomp on` their callers
get the wrong value — and 1 was never reached. Those 31 kept their old kind until the next change fixed them:
each now leaves v0 (and v1 for `func_088D9C70`) as the original does on every exit — often a leftover, such as the end
of a copied record, the vtable loaded for a tail call, an ll/sc success flag, or the last call's result — and is
registered `Word`/`DoubleWord`. `memset` was listed too: the caller at `0x08B2FCF0` begins with an lwl/lwr pair
into v1, which overwrites all four bytes; the audit now treats such a pair as a write and lists nothing. `func_0892C8B8` (a tail call nobody reaches on its own) is unregistered.
**Why it isn't obvious.** "Void" is our claim, not the original's: GCC leaves whatever the last call
returned in v0, and callers compiled against a non-void prototype read it.

Found: 2026-10-13 · host/diff_harness.cpp, src/game/unsorted/hot_leaves.cpp

## Purple videos and fades on the GPU: the first draw of a new texture got LCS's debug colour

**Symptom.** With `NativeGE=true` the intro movies and the logo fades came out violet or pink, the race sky
was flat cyan, and the first frame of a new screen was tinted. Not every run: the GPU gate failed different
frames each time.
**Cause.** `ge_gpu_backend_texture_available` required the texture's Vulkan image to exist, but a texture
decoded for this draw only gets its image when the frame is recorded. The draw counted as "texture not
ready", and `ge_renderer.cpp` then paints it with `gpu_texture_debug_color` — a colour hashed from the
texture's address, a debugging aid from LCS. Every movie frame is a new texture — sceMpeg's HLE writes
8888 to the decode buffer and GT draws a 5650 texture from another address (`GT_GE_LOG` at vblank 3145:
`tex @097df980 fmt 0`), so GT converts each frame — and every movie frame was tinted.
**Fix / workaround.** lcs-recomp patch 0024: a texture with decoded data waiting for upload is available.
If its upload then fails, the draw binds the white texture (untextured) instead of the debug colour.
**Why it isn't obvious.** The tint looks like a colour-format bug (a 5650/5551 mix-up, a blend constant), and
it varies between runs. A forced re-check of the texture contents changed nothing; `GT_GE_LOG` on the
failing frame's vblank (`[gpu-dump] frame N vblank V` in the dump log) showed a plain textured draw.

Found: 2026-10-14 · tools/patches/lcs-recomp/0024-gpu-new-texture-not-debug-colour.patch

## A slow GPU slowed the whole game down, not just the picture

**Symptom.** On a laptop with an integrated GPU (`NativeGE=true`), the car slowed down, frame pacing was bad and
the sound lagged; on a desktop GPU everything was fine.
**Cause.** At the end of each frame the Vulkan backend waited for the GPU to finish the previous one
(`vkQueueWaitIdle`, one frame in flight), and the game thread waits for the GE thread at its list syncs. GT
advances its logic and produces its sound per frame, so GPU time turned into game time. With
`GT_SIM_GPU_MS=20` (the GPU taken to need 20 ms per frame; race, averages over a minute) the game thread was
blocked 87.7 % of the time and the sound came out at 30.1 kHz instead of 44.1.
**Fix / workaround.** lcs-recomp patch 0026: the pending submit has a fence. A frame that only draws into the
two framebuffers shown last (GT's front and back buffer) waits at most 4 ms for the GPU and is dropped
otherwise, at most twice in a row; a frame that draws anywhere else (a reflection, an effect, an old display
buffer reused offscreen) still waits, since later frames may read it. The software path's present no longer
waits for the queue. At 20 ms per frame: blocked 33.4 %, sound at 42.6 kHz (without the 4 ms grace and the
safety rules it would be 1.8 % / 44.0 kHz, at the cost of needless drops on fast GPUs and of risking a render
later frames need). A fast GPU drops almost none; `GT_GPU_NO_DROP=1` turns dropping off (the GPU gate uses it,
so its runs don't depend on the machine's speed).
**Why it isn't obvious.** `guest time 1.00x` in `GT_PERF_LOG` still holds — the vblank clock keeps time — while
the game itself runs slow; the tell is `GE wait` and the audio rate. A fast GPU hides it entirely.

Found: 2026-10-14 · tools/patches/lcs-recomp/0026-gpu-drop-frames-not-game-speed.patch

## The software renderer's cost is its per-fragment pipeline, not decoding texels

**Symptom.** With `NativeGE=false` the software rasterizer uses about 2 cores at 480x272 (profile: per-pixel
loop, `sample_texture`, `read_clut_fast`, blending, writes).
**Cause.** Measured, not assumed: a cache of decoded RGBA textures (exact: 3.7 billion texels checked against
guest memory) removed the CLUT lookups (`read_clut_fast` 12.6 % → 1.6 % of samples) but saved only ~5-10 % of
CPU; the bilinear arithmetic, blending and writes dominate.
**Fix / workaround.** Not merged. A real gain needs a SIMD span rasterizer; the GPU path is the one to use.
**Why it isn't obvious.** The profile's top frames are texture functions; most of their time is the
filtering arithmetic, not the decode.

Found: 2026-10-14 · lcs/host/ge_renderer.cpp (lcs-recomp)

## Dealer car on the GPU: brighter and glossier, floor ring missing — what it is not

**Symptom.** With `NativeGE=true` the dealer's car looks brighter and glossier than on the software rasterizer,
the floor shows a bright glow beside the car instead of the turntable ring and shadow, and the plate text is
missing (GPU gate ×1, dealer route: 515/1763 frames equal; race ~93 %, garage 1776/1813).
**Cause.** Not found yet. Ruled out by experiment: the GPU hardware transform (`PSPRECOMP_GE_GPU_HW_TRANSFORM=0`:
514/1762), the GPU lighting (`PSPRECOMP_GE_GPU_HW_LIT=0`: 510/1762), the blend states the dealer uses
(`GT_GPU_STATS` on the route: only add with `As / 1−As`, `As / fix FFFFFF` and `min(1, 2As) / min(1, 2(1−As))` —
the last computed in `ge.frag blendFactor` with the same formula as `ge_renderer.cpp blend_factor`), stencil
(neither path implements it) and render-target aliasing (`find_target` keys on the address masked to
`0x001FFFF0`; no target sits at the environment map's address). Followed up
with `GT_GE_LOG_DETAIL=1`: the 257 reflection draws per frame use environment mapping (`texmap 000202`: mode 2,
UVs from the normalized normals and the lights `texshade` names), blend As / fix FFFFFF and color doubling, sampling a DXT1 map whose address
moves (`0x040D4000`, `0x040CC000` — the latter where a display buffer once was, so the GPU took it for that
render target); both paths compute the env-map UVs with the same formula, and refusing DXT/CLUT textures as
render targets changed nothing measurable (517/1758) — not merged. Every draw of that frame targets the shown
buffer (no offscreen render-to-texture). Still to find: the turntable ring, the plate text and a glow beside the
car.
**Fix / workaround.** None yet; documented as the GPU path's known difference.
**Why it isn't obvious.** Everything else in the dealer frame matches; only the reflective surfaces differ.

Found: 2026-10-14 · tools/src/lcs-recomp/lcs/host/ge_gpu_backend_vulkan.cpp, ge_renderer.cpp


## GPU: the menu's "Single Player" label drawn on the kerbs, an old track map in the race

**Symptom.** With `NativeGE=true` a track's kerbs showed the main menu's "Single Player" label, and the race's
track map was another picture (a "T").
**Cause.** The GPU backend keys render targets by their 21-bit VRAM offset (the GE framebuffer pointer is one),
and looked up a texture's target with its address masked to 21 bits. A texture in main RAM (0x09xxxxxx) whose low
bits matched a target's offset was drawn from that target's image instead — whatever the menu had rendered
there. Only VRAM (0x04xxxxxx and mirrors) can hold a render target.
**Fix / workaround.** lcs-recomp patch 0029: `texture_target` refuses non-VRAM addresses. GPU gate, race route,
fresh save: the track map matches the software rasterizer.
**Why it isn't obvious.** Patch 0027 (#163) first blamed the texture cache's sampled content hash and was
"verified" with a GPU gate run that never reached the race: `gt_play` keeps its save in `build/savedata/`, and
earlier Options-route runs had changed it, so the race route's presses landed in the Options menu. The gate
now gives every run a fresh save (`GT_SAVEDATA_DIR`). 0027 (hashing whole textures) stays as a cheap
safeguard, but it did not fix this.

Found: 2026-10-14 · tools/patches/lcs-recomp/0029-gpu-only-vram-textures-are-targets.patch, tools/gpu_gate.sh

## GT_PAD times are vblank waits, not presented frames

**Symptom.** A new pad route that worked step by step went astray when extended: presses landed in a loading
screen or in the wrong menu, and the frame-dump numbers didn't line up with the times in `GT_PAD`.
**Cause.** `GT_PAD` counts vblank waits (`pad_script_tick`); `GT_FRAME_DUMP` numbers presented frames. The menus
present one frame per two vblank waits, and loads run thousands of waits with few presents, so the two clocks
drift apart (in the Options route, frame 6500 was vblank 13 448). A press timed by frame number fires early.
**Fix / workaround.** Frame dumps now print `[frame-dump] frame N vblank V` on stderr: time presses by `V`.
Also in the Options menu, a second DOWN from the last row selects *Reset to default* and then *Cancel*, and
leaving asks "Save changes?" (one more CROSS).
**Why it isn't obvious.** In the first minute (logos, title) the two counts are close, so early routes worked.

Found: 2026-10-14 · host/pad_script.cpp, host/frame_dump.cpp, config/pad_routes/options.pad
## Movies broke up: the decoder dropped every picture it was handed while still holding one

**Symptom.** The intro movie (and the others) showed smeared, blocky pictures, and the log filled with
`[h264] deblocking filter parameters … out of range` / `no frame!` / `concealing … errors` (78 lines in 90 s).
**Cause.** `MovieDecoder` feeds each 2048-byte ring packet through the H.264 parser and `avcodec_send_packet`.
GT's movies have small pictures, so one packet often completes several; while the decoder still holds an
undelivered picture, `avcodec_send_packet` answers `EAGAIN`, and the return value was ignored — that picture was
lost, and every picture predicted from it decoded from the wrong reference. The bytes were right: the same
elementary stream, dumped and decoded by the `ffmpeg` CLI, has no errors.
**Fix / workaround.** On `EAGAIN` the decoder takes the waiting picture out (kept in a queue `next_frame` serves
first) and sends the packet again. `host/tests/movie_decoder_test.cpp` (synthetic libx264 stream, several
pictures per packet): 4/60 → 59/60 pictures. In the game: 0 decoder errors.
**Why it isn't obvious.** The demuxer was the suspect, and a stream dump decodes cleanly; only the send/receive
contract of libavcodec explains why the same bytes fail inside the game.

Found: 2026-10-14 · host/movie_decoder.cpp

## The GPU path's race ran at half speed because of the GE thread, not the GPU

**Symptom.** With `NativeGE=true` a race ran at ~30 presented frames/s with audio at 13–22 kHz (the laptop:
~27 fps plugged in, ~10 on battery), at any internal resolution — x1 as slow as x8 on an RTX 3080 Ti.
**Cause.** Profiled (gperftools, `-g` on the lcs host sources, race 170–210 s, fresh save): the GE worker thread
was ~92 % busy and the guest waited on it. A fifth of it was hashing: per draw, the palette checksum (byte by
byte) and the content hash of every small texture, both a serial mix per 8 bytes; and lcs's static vertex cache
hashed every cached draw's vertex bytes each frame, costing more than decoding them again.
**Fix / workaround.** lcs-recomp patch 0030: a four-lane hash (`fast_hash_bytes`), and texture/palette hashes
remembered within one display list until a block transfer writes memory (`ge_renderer_forget_content_hashes`,
called by GT's list executor). `host/main.cpp` turns the static vertex cache off (`PSPRECOMP_GE_VERTEX_CACHE=0`,
`=1` restores it). Race at x1 (`tools/gpu_perf.sh`): 43 → 57.5 presented frames/s, audio 29 → 42 kHz, skipped
vblanks 22 → 2.8/s; at x4 54 frames/s, 39 kHz. GPU gate, same counts before and after (race 339/744 ≈ 340/772,
dealer 174/586 ≈ 175/585, garage 12 failing frames in both).
**Why it isn't obvious.** The GPU's own time (`GT_PERF_LOG`'s GPU line) was small; the GE wait looked like
waiting for the GPU, and the internal resolution changed nothing. Only a profile of the GE thread showed host
bookkeeping per draw.

Found: 2026-10-14 · tools/patches/lcs-recomp/0030-ge-thread-cheaper-content-hashes.patch, host/main.cpp

## A `lui`/`addiu` pair in the ELF is relative to a segment, not to the load base

**Symptom.** Decoding the static addresses of a function from its raw words gave values 0x4D8500 below the ones the
generated code and the game use (`tools.family_members`, 2026-10-14).

**Cause.** The `.rel.text` entries (type 0x700000a0 section, 8-byte Elf32_Rel) carry the segment an address is relative
to in bits 16-23 of `r_info` (`0x10005` = R_MIPS_HI16 against segment 1). Segment 1 (data/bss) has `p_vaddr` 0x4D8500, so the
runtime address is `(hi << 16) + signed(lo) + p_vaddr[segment] + 0x08804000`. Calls (`R_MIPS_26`) are segment 0.
A pair without a HI16/LO16 relocation is a plain constant (a float bit pattern, a mask), not an address.

**Fix / workaround.** Read the relocation's segment and the program headers (`tools/family_members.py`).

Found: 2026-10-14 · tools/family_members.py

## Where the GPU's time goes in a race: ten fragments per pixel, a third of them clears

**Symptom.** At x4 the laptop's Vega 8 spent ~14 ms per race frame; a PSP scene should cost far less.
**Cause.** Measured with Vulkan pipeline statistics (`GT_PERF_LOG`'s GPU line, `GT_GPU_OVERDRAW=1` by kind of
draw): ~10.5 fragments per pixel of the frame, ~40,000 primitives. Per frame, GT clears depth (and some colour)
with clear-mode rectangles over the whole screen several times, and draws a few full-screen 3D quads without
texture or depth test (the backdrop). Each clear went through the full fragment shader. `gl_FragDepth` (patch
0025, 16-bit depth) was suspected and measured: 3 %, not the cause (and it is what makes depth match the PSP).
**Fix / workaround.** lcs-recomp patch 0031: a clear-mode rectangle that is one axis-aligned rectangle with one
colour and one z, writing all colour channels or none, becomes `vkCmdClearAttachments` (same pixels by the
rasterizer's coverage rule; the depth ge.frag would write). Race x4: 10.5 → 8.85 fragments per pixel, GPU
4.3 → 4.06 ms on an RTX 3080 Ti (fixed-function clears are relatively cheaper on AMD). GPU gate on the frames
both runs captured: race 340 vs 339 equal, dealer 175 → 174, garage 12 failing frames in both.
Clears of RGB only or alpha only stay draws (vkCmdClearAttachments can't mask channels).
**Why it isn't obvious.** The GPU timestamps alone said "slow"; only counting fragments by kind showed that a
third of the work was clears.

Found: 2026-10-14 · tools/patches/lcs-recomp/0031-gpu-overdraw-counters-and-clear-rectangles.patch

## A race records ~10,000 draws a frame: the driver's per-draw calls were 13 % of the CPU

**Symptom.** After the GE thread's own costs were cut, a profile of a race (gperftools, race 170–210 s) still
showed 13 % of all samples inside the Vulkan driver (libnvidia-glcore) and 16 % in libc.
**Cause.** GT issues ~10,000 small draws a frame (~4 triangles each); the backend recorded for every one of them
a pipeline bind, the vertex and index buffer binds, a scissor, blend constants and a descriptor-set bind with a
fresh uniform slot — mostly the same state as the draw before.
**Fix / workaround.** lcs-recomp patch 0032: the geometry buffer is bound once a frame; pipeline, scissor and
blend constants only when they change; a draw with the same uniforms and texture as the one before reuses its
slot and binding. Race at x1, same profile window: process CPU samples 41,382 → 37,306 (−10 %), driver
5,380 → 2,163 (−60 %). GPU gate race: 341 vs 339 frames equal on the 864 both runs captured.
**Why it isn't obvious.** The driver's time is outside the game's binary and shows only as a library in a
whole-process profile; per-draw state is cheap per call and expensive at 10,000 calls a frame.

Found: 2026-10-14 · tools/patches/lcs-recomp/0032-gpu-record-only-state-changes.patch

## The kerbs' "Single Player" again: a stale render target, not only the address mask

**Symptom.** After 0029 (only VRAM textures can be targets) the track map was right but the user still saw the
menu's "Single Player" label on the kerbs of the first race (not on the gate's race route, which never shows those
kerbs close).
**Cause.** The kerbs' texture is in VRAM, where the menu once rendered that label. The game loads the texture over
those bytes with the CPU — which the GPU backend never sees — so the backend still took the address for the
menu's render target and sampled its old image.
**Fix / workaround.** lcs-recomp patch 0033: a texture reads a render target only if the GE drew into it within
the last 60 frames (one second; the menu's targets are long stale after the loading screen). Effects that sample
a target (mirror, environment maps) draw into it every frame or every few. GPU gate, frames both runs captured: dealer mean difference
0.0311 → 0.0157, garage 0.0036 → 0.0025, race unchanged (341 equal) — stale targets were also some of the
dealer's differences.
**Why it isn't obvious.** The same symptom had two causes (the masked address, then the stale target), and the
gate's routes only exercised the first.

Found: 2026-10-14 · tools/patches/lcs-recomp/0033-gpu-stale-render-targets.patch

## An integrated GPU on battery needs a lower internal scale, chosen at run time

**Symptom.** On the laptop (Vega 8), x4 at 1080p cost 14–18 ms of GPU per race frame on battery: the GE thread
waited in present and the race ran at ~27 fps with audio at 14 kHz, while the menus ran at 60.
**Cause.** Pixel cost: ~9 fragments per pixel at 1920x1088. The CPU side was no longer the limit there.
**Fix / workaround.** lcs-recomp patch 0033 (second part), `Rendering.GpuBudgetMs` (default 10, 0 = off): in Window mode the
whole scale steps down while the measured GPU time per frame exceeds the budget and back up when the next scale
would fit in 70 % of it (`budget_scale_cap`, unit-tested in host/tests/internal_resolution_test.cpp). Headless
runs have no swapchain (no Window mode), so this one is checked on a real window — the laptop.

Found: 2026-10-14 · tools/patches/lcs-recomp/0033-gpu-stale-render-targets.patch, config/gt_play.ini

## The GPU path cleared the shown framebuffer every frame; the PSP never does

**Symptom.** With `NativeGE=true`: a black frame between menus (only the cursor drawn), "the picture flickers"
and pieces flicker (laptop report); in the GPU gate, isolated frames 0.71–0.73 different from the software
rasterizer at menu transitions.
**Cause.** lcs-recomp's backend (written for GTA, which redraws the whole screen) cleared the display target at
the start of every frame. The PSP keeps a framebuffer's contents until something draws over them; GT redraws only
part of the screen in some frames (a cursor over the last menu picture).
**Fix / workaround.** lcs-recomp patch 0034: only a new target starts cleared (`GT_GPU_CLEAR_DISPLAY=1` restores
the old clear). GPU gate: race isolated spikes 3 → 0 and mean difference 0.0445 → 0.0429; dealer unchanged; one
garage fade frame differs in its darkening (0.33), where the previous run matched.
Also patch 0035: the present shader clamps its bilinear filter to the shown crop — the last row blended in what
lay outside the picture in the larger target, a thin line of changing colour at the bottom of the window.

Found: 2026-10-14 · tools/patches/lcs-recomp/0034-gpu-keep-display-contents.patch, 0035-present-filter-inside-crop.patch
