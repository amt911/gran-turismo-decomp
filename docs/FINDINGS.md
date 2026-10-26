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

### Images 60/s, screen 30/s: KDE's Night Color made KWin compose every frame (and the 59.94/60.05 Hz beat)

**Symptom.** On the laptop (KWin 6.8, Renoir, power-saver) the menu ticker and passing cars stuttered with the game at
a perfect 59.9 images/s (intervals p99 17.5 ms). Screen-side times (`GT_FRAME_LOG` D rows, VK_KHR_present_wait) showed
one image every two refreshes, the other replaced unseen; in FIFO, every image held for two refreshes. Halving the GPU
work (scale ×2) changed nothing; the performance profile helped only partly, and then the losses came in bursts every
~9 s.
**Cause.** Two. (1) Night Color on: KWin went from 0.2 % to 33 % CPU with the game (10 % with it off), i.e. it
composed the fullscreen window every frame instead of scanning it out, and with power-saver clocks (GPU at 400 MHz,
CPU 1.9 GHz) that composition missed the refresh, so KWin ran at 30 Hz. Night Color off, same power-saver: 5091 of
5103 images exactly one refresh apart. (2) The guest's 59.94 Hz against the 60.05 Hz panel: every 9.2 s the present
slides across KWin's deadline and for a second or two images land late or get replaced (bursts at a 9 s period in the
per-second loss share).
**Fix / workaround.** (1) Not ours: switch Night Color off (or play in performance) — written in
`docs/PRUEBAS_PORTATIL.md`. (2) `gt::DisplayPhase` + `VblankPacer` lock guest vblanks to the screen's refreshes
(present_wait times; game 0.18 % faster on a 60.05 Hz panel, audio absorbs it, patch 0055): losses 64 → 12 per 85 s.
Present wait needs a Vulkan 1.1 instance (patch 0054): under 1.0, `vkGetPhysicalDeviceFeatures2` reports presentWait
absent on RADV.
**Why it isn't obvious.** Every client-side number (present cadence, GPU time, vblank lag) was clean; only the
screen-side times showed the loss, and a compositor setting decided it. `perf` cannot attach to kwin_wayland (it has
capabilities, so it is non-dumpable): compare its CPU in `top` with and without the game instead.

Found: 2026-10-22 · runtime

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

## The frameskip only skipped software rasterizing; with the GPU on, a slow CPU never caught up

**Symptom.** Laptop on battery, `NativeGE=true`: the race at ~27 shown fps with uneven pacing, audio produced at
13–14 kHz instead of 44.1 kHz (it stutters and cuts out), `GE wait` 74 % and `frameskip 26/s` in the perf log —
the frameskip was on most frames yet nothing got faster. Reproduced on the desktop by pinning gt_play to two
cores (`taskset -c 0,1`): race audio 32–35 kHz with `GT_FRAMESKIP=0`.
**Cause.** `g_frameskip` was read only by the software rasterizer. On the GPU path every draw of a skipped frame
was still decoded, accumulated and recorded, so the GE worker (the bottleneck: the game thread waits for it before
each list) did the same work whether the frame was skipped or not.
**Fix / workaround.** lcs-recomp patch 0036: when the frameskip is on, a draw into one of the two shown screens
only advances the GE's vertex/index pointer (render-to-texture draws still run). The backend presents only frames
that drew into the display target, so the window keeps the last picture. Two cores, race: audio 44 kHz, GE wait
27–37 % (was 59–62 %), GE frames 75–78/s (was 53–64). The same patch cuts per-draw CPU: a job prepared and
committed at once points at the caller's state instead of copying several KB, and a triangle list with the same
descriptor and transform as the previous batch joins it, unless it samples a render target (each such batch
snapshots its target first) (gperftools race samples 36,677 → 35,323, `tools/ge_profile.sh`). The host decides the
skip once per frame (between two flips), so a frame whose lists straddle a vblank wait is never shown half drawn.
The GPU gate runs with `GT_FRAMESKIP=0`, so the frameskip is checked on the laptop; the batch merging is in the gate.

Found: 2026-10-14 · tools/patches/lcs-recomp/0036-gpu-frameskip-and-cheaper-draws.patch

## Distant textures shimmered at high internal scales: the mip level was measured in the wrong pixels

**Symptom.** Laptop screenshot at x3/x4: distant signboards and barriers aliased ("se renderizan mal en la
distancia"). At x4 the race's GPU frames, box-downsampled to 480x272, differed more from the x1 GPU frames than they
should.
**Cause.** In the automatic mip mode the PSP picks the level from the texels per pixel of its own 480x272 screen
(PPSSPP's software GPU: `log2(max(ds/dx * width, dt/dy * height))` + the level offset). Vulkan measures that slope
in internal-resolution pixels, N times smaller at xN, so it sampled log2(N) levels finer than the PSP.
**Fix / workaround.** lcs-recomp patch 0037: in the automatic mode the sampler's LOD bias adds log2(N) (the bias is
part of the sampler key, so a scale change makes new samplers). x1 is unchanged, so the pixel gate is unchanged;
race at x4: closer to the x1 picture in 399 of 418 frames (mean |x4 - x1| 13.06 → 11.63).
**Also found.** lcs-recomp's software rasterizer (the gate's reference and `NativeGE=false`) always samples
level 0 in the automatic mode (`selected_texture_level` returns 0 unless the mode is constant), which the PSP does
not do: part of the race's GPU-vs-software differences are the GPU being closer to the PSP. Not fixed yet.

Found: 2026-10-15 · tools/patches/lcs-recomp/0037-mip-level-at-psp-resolution.patch

## A race frame was ~8,800 draws because each object's LOD offset made its own texture

**Symptom.** After merging draws with equal state (0036) a race frame still recorded ~8,800 batches of ~4
triangles, and recording took 1.8 ms of the GE thread per frame on the desktop (2 cores pinned) — 8.4 ms on the
laptop, measured as "present". Counting what changed between consecutive batches: the texture binding changed
on 8,700 of them, the uniforms on 250, the pipeline on 113.
**Cause.** A race frame samples ~300 images (address + content + palette), but the texture key and the sampler
also held the draw's mip level selection (the PSP's LOD offset, which GT sets per object, the level mode and the
fixed level): ~1,500 textures, so almost every draw switched texture and none could merge.
**Fix / workaround.** lcs-recomp patch 0039: the level selection is a vertex value (`UploadVertex::lod`, 36 → 40
bytes) that `ge.frag` applies (`texture(..., bias)`, or `textureLod` for the fixed level a render target sampled
as a texture pins). Vulkan adds the shader bias to the sampler's and clamps the sum the same way. The samplers keep
no bias, the keys no level selection, and the merge ignores those fields. Two cores, race: finish 1.85 → 0.47 ms
per frame (record 1.75 → 0.37), GPU 3.1 → 2.5 ms per frame, GE wait ~28 → 20 %. GPU gate vs the run before on common frames: race 339/857 passing (341), mean 0.0430 → 0.0434, no frame over 0.3; dealer 174/587 (174); garage 594/605 (594). The frames that changed moved both ways (run-to-run timing).
`GT_PERF_LOG` now also splits the GE thread (patch 0038): `lists` (decoding and accumulating draws) vs `finishes`
(record, submit, present).

Found: 2026-10-15 · tools/patches/lcs-recomp/0038-ge-thread-time-breakdown.patch, 0039-lod-selection-in-vertices.patch

## A tail call to an import stub leaves v0 as the last load set it: func_08B34C1C mismatched on every case

**Symptom.** A hand-written function ended with `tail_call(ctx, 0x08C7A8A8, {flags})` (the resume-interrupts import) and the
diff harness reported `v0: original 0x0, decompiled 0x1` on all of its cases, although the call sequence matched.
**Cause.** The HLE of `sceKernelCpuResumeIntr` (`host/gt_kernel.cpp`) does not write v0, so the function's result is
whatever v0 held when the original jumped to the stub: the word its last `lw v0` loaded on that path (a field
tested, or compared, just before). The decompiled code left the case's entry v0.
**Fix / workaround.** Set `ctx.gpr[2]` before the tail call to what the original's last load leaves on each path (this
function: `this[8]`, then `this[6]`, then the old tail). The same holds for any import whose HLE returns void.

Found: 2026-10-15 · src/game/unsorted/hot_batch_76.cpp (func_08B34C1C)

## Two GPU gate runs don't race the same frames: compare a run's GPU against its own software frames

**Symptom.** Patch 0040 (two memos, provably equivalent) seemed to cost the race 30 passing frames (339 → 309,
30 worse, 0 better than the run before).
**Cause.** The run's software frames differ from the previous run's at the same index just as much as its GPU
frames (mean |diff| ~29 per byte averaged over 88 sampled frames, up to ~97 on single frames; 4 of 88 identical): loading took a different time, so the race started ~900
frames earlier and 30 pre-race frames (which pass) became race frames (which never pass: the software reference
samples mip level 0 in the automatic mode — see "Distant textures shimmered"). Pass counts across runs measure
when the race started as much as the renderer.
**Fix / workaround.** Judge a renderer change by windows of the same content (pre-race frames, race frames) and
by each run's GPU-vs-software agreement, not by the pass count difference between runs; a CPU-side memo is
checked by reading it for equivalence. Patch 0040 itself: the texture key (~50 hash mixes a draw) is reused when
the descriptor equals the previous draw's, and the two 4x4 matrix products when world, view and projection are
unchanged; the race profile lost `multiply_mat4` and half of `hash_mix` (process samples over the same 40 s:
28,411 → 25,903), the GE thread's `lists` on two cores did not move beyond noise.

Found: 2026-10-15 · tools/patches/lcs-recomp/0040-per-draw-memos.patch

## `tools.family_members --list --any-template` lists families that earlier batches wrote and left unregistered

**Symptom.** Batch 77 wrote the "clamp" family (func_08C401C4 and four instances) and "release a vector" (func_08C4B740 and three more) from scratch, and
only afterwards found them in batches 65 / 68 / 73 / 74, written and commented out because no compared case changes memory.
**Cause.** The tool counts as done only the *active* registrations (and `--exclude-ref` branches), so a function written but left unregistered is
pending again, and a family with no registered member shows up as new.
**Fix / workaround.** Before reading a family, `grep -rn <address> src/` for its members: a hit in a commented-out `HOTnn(...)` line is a function that
was already tried. The decision made there (no case changes memory: stays recompiled) holds until a route reaches the non-empty case.

Found: 2026-10-15 · src/game/unsorted/hot_batch_77.cpp

## The race fails the GPU gate mostly on mip levels: GPU_GATE_LOD0=1 makes it a check again

**Symptom.** Every race frame failed the GPU gate in every run (0 of 100 per 3,000-frame window, mean
difference 0.05-0.09), so the race could not show a regression; only the pre-race frames passed.
**Cause.** Mostly the reference, not the GPU: the software rasterizer samples level 0 in the automatic mip mode
(`selected_texture_level`), while the GPU picks the level as the PSP does. Forcing the GPU to level 0
(`TextureLodBias=-8`): race windows 0.05-0.09 → 0.016-0.043 mean difference and 11-22 of 100 frames passing
where none did. The rest (0.016-0.043, alternating by window) has other causes, not investigated yet.
**Fix / workaround.** `GPU_GATE_LOD0=1 tools/gpu_gate.sh ...` writes `TextureLodBias=-8` into the run's ini: use it
to check a renderer change in the race (everything but the level choice). The real fix is the automatic level in
the software rasterizer (per-pixel derivatives in every raster path), which also is what `NativeGE=false` shows.

**Update 2026-10-19.** The race now samples level 0 by default in `tools/gpu_gate.sh` (923/923 frames pass on a 600 s run). The dealer and garage keep the real levels, which they pass since patch 0043. `GPU_GATE_LOD0=1` forces level 0 on every route and `GPU_GATE_LOD0=0` on none.

Found: 2026-10-15 · tools/gpu_gate.sh

## A callee that sets no v0 hands the caller's v0 on: a decompiled function must keep the previous call's results in the registers

**Symptom.** Batch 80's first gate run: `func_089FE57C` ended with `v0: original 0x0, decompiled 0x089FE57C` and `func_088B46A8` with `v0: original 0x093D6EF4, decompiled 0x088B46A8`
(the decompiled value is the function's own address, the v0 it was entered with), although every call and every store matched.
**Cause.** The last guest call of both is a void leaf that never writes v0. In the original, v0 at that point is whatever the call before it returned, and the function leaves
with it. `call_guest` starts the callee from the caller's `ctx`, and a decompiled function that only keeps the previous result in a C++ variable leaves `ctx.gpr[2]` at the entry value.
**Fix / workaround.** Every call goes through a wrapper that copies the callee's v0, v1 and f0 into `ctx` right after it (`call_keep` in `src/game/unsorted/hot_batch_80.cpp`), so the next callee
and the epilogue see what the original's registers held. Where the original overwrites v0 itself between two calls (a load, a constant), set `ctx.gpr[2]` there by hand.

Found: 2026-10-15 · src/game/unsorted/hot_batch_80.cpp (func_089FE57C, func_088B46A8)

## The call probe does not count a call made from inside the same generated unit: a hot function can be missing from it

**Symptom.** `func_08B8705C` (43 instructions) never appeared in any `probe_*` log, although its only caller `func_08B87108` ran 4,855 times; in the fast gate, once registered,
it was compared 13 times with 13 equal.
**Cause.** The probe counts entries through the dispatcher; a `jal` between two functions of the same generated unit is a direct jump (the reason `tools.recomp` has to be rerun after
every registration), so a callee that only its own unit's functions call has `hits=0`.
**Fix / workaround.** When a family member that was read has a hot caller, look at the caller's `jal` targets: the callee may be a sibling the probe never saw. A sibling is the same
family only if the tool says so: `func_08B8705C` moves a3 where the family of `func_08B86FB0` moves a2 (the first gate run mismatched on RAM in all 11 cases of the wrong reading).

Found: 2026-10-15 · src/game/unsorted/hot_batch_80.cpp (func_08B8705C, func_08B86FB0)

## A branch target counted by hand was one word off, and a delay slot reused a register: read the asm with `tools.disasm`

**Symptom.** Batch 81's first gate run: `func_08B54FB8` mismatched on RAM (`0x08D6CE71: original 02, decompiled 06`, state 0x8002: the original set bit 10 of the graphics state's flag word, the
decompiled code bit 7) and `func_08C436F4` answered 1 where the original answered 0 (21 of 29 cases). The coverage review of the untaken states then found a third: the states 0 and 1 leave with v0 = 0, not 1.
**Cause.** Three reading errors, none visible in the C: (1) for `beq $a0,$v0, . + 4 + (0x4 << 2)` I counted the target as `0x08B55034`, the instruction before the real `0x08B55038`; the skipped word is
`addiu $v1,$zero,7`, so the delay slot's `addiu $v1,$zero,0xA` stays the shift. (2) `lbu $v1,0x13($a0)` overwrote the handle `lw $v1,0x18($a0)` had put in v1, and the `addu $a0,$v1,$zero` in a later `jal` delay slot
passed the byte, not the handle. (3) The delay slot of `bnez` ran (`sltiu $v0,$v1,7`) before the return, so v0 was already that value, not the `sltiu` before the branch.
**Fix / workaround.** `.venv/bin/python -m tools.disasm ADDR [ADDR:N ...]` prints every branch as `-> ADDRESS` (resolved from the delay slot), the relocated `lui`/`addiu`/load addresses and the `jal` targets: take
targets from it, never count `pc + 4 + (N << 2)` by hand. When a register is reused between a load and a delay-slot use, follow the register, not the name in the comment; the delay slot of every branch is part of the
branch's path, taken or not (a likely branch skips it when not taken). The gate only catches the paths its cases take: read the untaken ones in the coverage list against the asm again.
Batch 87 added a fourth form: a `move s0, v0` in the delay slot of the `jal` that follows a call captures the PREVIOUS call's `v0` (`func_0880D478` kept the allocation of the next `jal` instead of the earlier answer and
faulted on all 6 compared cases), so a register written in a delay slot is read as of before the call it sits in.

Found: 2026-10-15 · src/game/unsorted/hot_batch_81.cpp (func_08B54FB8, func_08C436F4)

## A function that stores `ra` or `sp` (setjmp) cannot be registered: inside a harness case `ra` is the case sentinel

**Symptom.** Batch 82's first fast gates stopped every route after about a dozen compared functions with `Unsupported Allegrex instruction 0x00000000 at 0x0BAD0000: diff harness sentinel reached
without a pending continuation` (exit 2 or 3, 52 of 62 functions never compared). The one mismatch the first run did show (`func_08B34CA0`, a `v0` left by a load before a tail call) was not the cause;
dropping the functions whose cases stop at blocking imports (vblank wait, `sceIoRead`) changed nothing.
**Cause.** `func_08B7B2E4` is `setjmp`: it writes `s0-s7`, `sp`, `fp` and `ra` into the buffer it is given. A case runs the original and the decompiled version with `ra` = the case's sentinel
(`0x0BAD00xx`), so the buffer written into the live memory holds the sentinel as the return address; the game's later `longjmp` through that buffer returned to a sentinel that no case is waiting on. The
case itself compared equal (both runs stored the same sentinel), so the gate looked green for the function.
**Fix / workaround.** `setjmp`-like functions (anything that stores `ra` or `sp` to memory, or hands them out) stay recompiled; the batch file keeps the written version unregistered with this reason.
Found by dropping suspects in the order "stops at an import" then "stores `ra`" — read the buffer-writing functions of a batch before the first gate when the run dies at the sentinel.

Found: 2026-10-15 · src/game/unsorted/hot_batch_82.cpp (func_08B7B2E4)

## An import that is `replay_safe` still leaves the register that a tail call hands on: the stub sets no `v0`

**Symptom.** `func_08B34CA0` (the twin of batch 76's `func_08B34C1C`) mismatched all 4 first cases on `v0` (original 0, decompiled 1).
**Cause.** The function ends with `j sceKernelCpuResumeIntr`; the HLE of that import does not write `v0`, so the value the caller sees is the last load before the jump (the word `+0x20`, then `+0x18`,
then `+0x14` on the paths that go further). The decompiled version left the `v0` of the earlier `sceKernelCpuSuspendIntr` call.
**Fix / workaround.** Before the `tail_call` into an import stub set `ctx.gpr[2]` to the last value the original's instructions load into `v0` (as `func_08B34C1C` does).

Found: 2026-10-15 · src/game/unsorted/hot_batch_82.cpp (func_08B34CA0)

## `tools.decomp_gate coverage`: "never false at X" means the condition was always true

**Symptom.** Reading batch 82's coverage list, three comments were first written with the branches reversed (`func_08B18088`: "the routes take the answer 0 with a non-zero counter" when the counter was always non-zero and
the answer always 0 was not what the list said; `func_08C21C84`, `func_08B34CA0` likewise).
**Cause.** The list names the *outcome that never occurred*: "branches never false at 845:9" for `if (object != 0u)` is a condition that was true in every compared case, not a branch that was not reached.
**Fix / workaround.** Turn each entry into "the routes always take X; Y is not taken" before writing the comment and check the Y path against the asm; a `true`/`false` pair at the same column with a
`regions never run` entry means the operand was never even evaluated.

Found: 2026-10-15 · src/game/unsorted/hot_batch_82.cpp

## A decompiled function can switch off code the gate never sees: watch the compared count between full gates

**Symptom.** Batch 83's full gate: 0 mismatches, but 4,761 functions compared against 5,034 the batch before, the
same on a rerun at low load. 328 functions that ran exactly once on every route (start-up code, not called
directly: through a table) now ran 0 times; the routes' total call volume was unchanged.
**Cause.** Installing `func_08A9DD14` alone reproduces it (bisected in 25 s runs of `gt_native --decomp diff` with
`GT_DECOMP_ONLY`, counting the calls of one of the lost functions, `func_0880A500`). Its body matches the asm word for
word (`tools/disasm`); what differs is that its calls run nested through `call_guest`, and one of them
(`func_08A9C340`, through a virtual-call chain) evidently does not behave as when the original jumps to it.
The gate compares each call's result, not what never runs afterwards, so it stayed green.
**Fix / workaround.** `func_08A9DD14` stays written but not registered. A batch's full gate is also judged by its
compared count against the previous main: a drop of hundreds is a behaviour change to bisect (`GT_DECOMP_ONLY` with
halves of the batch), not noise. Root cause of the nested-call difference not established yet.

Found: 2026-10-15 · src/game/unsorted/hot_batch_83.cpp

## Noise on distant signboards: GT's small mip levels are not the bigger one halved

**Symptom.** Laptop at 1080p after 0037: lettering on distant banners (TRD, elf) with black lines; at x4 the Gran
Turismo logo banners came out as red/blue noise, correct with `TextureLodBias=-8` (level 0 only).
**Cause.** A PSP mip level has its own size registers, and GT keeps the small levels 32 texels wide (128x128 →
64x64 → 32x32 → **32x16 → 32x8**, CLUT4/CLUT8). The decoder packed each level at its own size, while the upload
copies level n as level 0 halved n times: every level after the first odd one was read at the wrong stride and
offset. 0037 made the game pick those levels as the PSP does, which put the noise on screen.
**Fix / workaround.** lcs-recomp patch 0041: a level of another size is box-resampled to the halved size (assuming the
PSP samples a level with normalised coordinates, not verified on hardware). GPU gate x1: every race window closer to the software reference
(mean 0.086 → 0.076, 0.057 → 0.044, 0.087 → 0.075, 0.061 → 0.047, 0.091 → 0.074), dealer windows 0.022 → 0.016,
garage unchanged.

Found: 2026-10-15 · tools/patches/lcs-recomp/0041-mip-chain-own-level-sizes.patch

## The laptop spent 3 ms per frame recording even in menus: measure the windowed path, not only headless

**Symptom.** Laptop log with 0038's split: `record 3.0 ms` per frame in menus with 6 primitives, where a headless
race on the desktop records in 0.35 ms. The headless gate and `tools/gpu_perf.sh` never present to a window,
so the present path (swapchain checks, overlay, acquire) and the window-only per-frame work were never timed.
**Cause (being narrowed).** Per frame the backend asked the driver and SDL for the surface capabilities, formats and
window size (`ensure_swapchain`, round trips to the display server on X11), and every texture upload allocated, mapped
and later freed its own staging buffer (a kernel call each on an integrated GPU's driver). On the desktop with a
window those cost little; on the laptop they are suspects, not yet proven.
**Fix / workaround.** lcs-recomp patch 0042: one persistent 8 MB upload buffer reused every frame (the previous
frame's fence is waited before recording), the swapchain checks skipped while the window size is unchanged and
nothing marked it out of date (`VK_SUBOPTIMAL_KHR` sends the next present back to the full check), and
`GT_PERF_LOG` splits `record` into uploads, draws and the present pass (the three don't add up to all of it), so the
next laptop log names what is left. GPU gate x1: identical windows and pass counts to 0041.

Found: 2026-10-15 · tools/patches/lcs-recomp/0042-fixed-frame-costs.patch

## Caching decoded geometry made the GE thread slower: GT's draws are tiny, the cost is per draw

**Symptom.** 99.8 % of a race frame's vertices are byte-identical to the previous frame's, so a cache of each draw's
converted vertices and indices (keyed by a hash of its inputs, `docs/superpowers/specs/2026-10-15-gpu-geometry-cache-
design.md`) looked like half of `lists`. Built and exact (verify mode: 0 mismatches over ~1M hits/s), it measured
**slower**: two cores, race, `lists` 404 → 556 ms/s, GE wait 20 → 34 %.
**Cause.** A race draw has ~14 vertices. Decoding reads 10-14 compact bytes per vertex; a hit copies 40-byte
converted vertices out of a ~30 MB scattered table and adds a lookup. The cost is the fixed work of each of ~660,000
draws per second (~600 ns each on the desktop), not the vertices.
**Fix / workaround.** Not merged. Cut the per-draw overhead instead (descriptor copies and comparisons, per-draw
allocations, backend queries); measure any cache against `lists` on two cores before keeping it.

Found: 2026-10-15 · docs/superpowers/specs/2026-10-15-gpu-geometry-cache-design.md

## A callee can read the caller's `s0`: a decompiled constructor that leaves the entry `s0` in place mismatches on RAM

**Symptom.** Batch 84's first fast gate: `func_0880BF70` (a constructor: `func_08B28DCC(this)`, `func_089BD4DC(a1)`, `func_08869988(this + 0x30)`, tail `func_08A76AA0`) mismatched on both cases and a single-function run
(`GT_DECOMP_ONLY`) reproduced it: `RAM at 0x09C96964: original 00, decompiled A0 (306 bytes differ)`, 20,000 differing bytes in the dirty-page list, although the sequence of guest calls and their `a0-a3` were identical (a debug trace of every dispatch in both runs).
With the nested `func_08869988` installed the same bug showed as `D0` (`this + 0x30`) against `A0` (`this`).
**Cause.** The original keeps `this` in `s0` (and `a1` in `s1`) across the calls; a callee under `func_08B28DCC` (the lock created through the `0x08C7AAB0` stub) stores the caller's `s0` into a table. The decompiled function kept `this` in a C++ variable
and left `ctx.gpr[16]` at the case's entry value, so the stored word was a different one. `call_guest` starts the callee from the caller's context, so every register the original holds live across a call is part of what the callee sees.
**Fix / workaround.** Set `ctx.gpr[16]` / `ctx.gpr[17]` to the values the original keeps in `s0` / `s1` before the calls and restore the entry values before returning or tail-calling (the original restores them from its frame): `func_0880BF70`,
`func_08869988`, `func_08B1BC98`. When a mismatch has an identical call sequence, trace the dispatches of both runs (`rt.function_at` loop in `host/guest_call.cpp` and the harness loop) before reading more asm.

Found: 2026-10-15 · src/game/unsorted/hot_batch_84.cpp (func_0880BF70, func_08869988)

## A tail call into a `Void`-registered nested function leaks its `v0`: the caller mismatches on `v0` only in the full gate

**Symptom.** `func_08881C38` (tail call through a vtable slot to `func_088A599C`, `CarParameter::set_color`) was equal in all fast-gate cases and mismatched on `v0` in 3 routes of the full gate
(`original 0x1, decompiled 0x09D0B700`, case "nested 0x088A599C").
**Cause.** `set_color` is registered `GT_DECOMP_NESTED(..., Void)`: its `v0` is never compared, and in the decompiled run it leaves the entry `v0` (the object) where the original leaves 1, the `v0` of its last callee. A function that ends
with a tail call to it returns that `v0` to its own caller. `tools.contract_audit` does not follow tail calls through a vtable.
**Fix / workaround.** `func_08881C38` stays unregistered. A function that tail-calls through a vtable slot is only as exact as the callee it reaches in that case; check the nested callee named in `mismatch case: ... nested ...` before touching the caller.

Found: 2026-10-15 · src/game/unsorted/hot_batch_84.cpp (func_08881C38)

## The shared-body compare of a string release leaves its sentinel in `v0`: a function that ends with the release mismatches on `v0`

**Symptom.** Batch 86's first fast gate: `func_0899CD50` mismatched on every case with `v0: original 0x08D74FD0, decompiled 0x00000001` although every call, store and the RAM matched; the same bug hid behind
`func_08AF464C` (`v0: original 0x08CD72C0, decompiled 0x08CBF908`: a constant loaded into `v0` before a callee that sets no `v0`).
**Cause.** The copy-on-write string release `p - 0xC == D_08D74FD0 ? skip : refcount call` compares against the shared empty body with `lui v0 / addiu v0` and a `beq`; when the body is the shared one the function leaves
that address in `v0`, when it is not, `v0` holds the refcount call's answer. A decompiled function that ends with the release and only keeps the constant in a C++ variable returns the callee's `v0` or the entry one. The same goes for a
constant the original parks in `v0` before a call to a callee that never writes `v0` (`call_keep` hands the context's `v0` on, so set `ctx.gpr[2]` first).
**Fix / workaround.** `release_string` sets `ctx.gpr[2]` to the sentinel before the compare (`src/game/unsorted/hot_batch_86.cpp`); the variants that keep the sentinel in an `s` register (`release_body`) do not.
Read the register the compare uses, not only the branch.

Found: 2026-10-15 · src/game/unsorted/hot_batch_86.cpp (func_0899CD50, func_08AF464C)

## `tools.family_members --check` takes every one-line `void func_X(...) { name(rt, ctx, N); }` for a table row: a bit position in an `ext`/`ins` is not a value of the words

**Symptom.** `--check` reported `BAD func_089E18BC: argument 0 is no value of the member's words` for two accessors that differ only in the bit number (`ext a1, a1, 3, 1` / `ext a1, a1, 2, 1`).
**Cause.** The checker decodes `lui`/`li`/displacements/call targets of a member; a field of `ext`/`ins` (position, size) is not one of them, so a wrapper argument that carries it can never match.
**Fix / workaround.** A parameter that lives in an instruction field is a template argument (`flag_accessor<3u>(rt, ctx)`, not matched by the row regex), the rest (call targets, constants) stays a wrapper argument and is checked.
Also: `GT_DECOMP_DENSE=0xADDR` is inherited by `tools.decomp_gate fast/coverage` (the harness reads it), so a hot function whose compared sample misses a path can be run dense through the tool; the case budget stays 32.

Found: 2026-10-15 · src/game/unsorted/hot_batch_86.cpp (func_089E18BC)

## A registered parent swallows the compared calls of its decompiled descendants: the full gate's "compared" total falls without any behaviour change

**Symptom.** Batch 86's first full gate (46 functions registered then, `func_0880BEF8` among them): 5,012 reached with 28 of the 46 among them, i.e. 4,984 without the batch against 5,039 for main (a run of the same tree with the batch's file moved away): 56 functions
that main reaches (the destructor tree of a menu object: `func_0886A858`, `func_0886BF44`, `stride_walk` instances, `member_forward`, ...) showed `calls 0`. Bisected with `GT_DECOMP_ONLY` (the lost function plus halves of the
batch, counting its `calls` on the four menu routes: 3 per route in main) down to one function: `func_0880BEF8`, the destructor of the menu object, called once per route, installed alone made all of them 0.
**Cause.** Not a behaviour change: the destructor's single call per route is compared as a case (equal, 4/4), and everything it calls runs inside that case; a callee that is registered but not `GT_DECOMP_NESTED` is not
counted there (only the dispatcher's own calls are), so its `calls` and `compared` drop to 0 although it ran. The same shadowing hid eight destructors of the batch itself in the first fast gate (`compared 0`) until the parent was
left recompiled. With it unregistered the final tree (51 functions, merged with batch 85) reaches 5,055 in the full gate, 0 mismatches.
**Fix / workaround.** `func_0880BEF8` stays written but unregistered (its descendants stay compared, and six of the batch's destructors went from 0 to 4-8 cases). A drop of the full gate's compared total is a behaviour change
only when the lost functions are not callees of a registered function whose cases ran: list them (`main` vs the branch, per address) and bisect with `GT_DECOMP_ONLY` before suspecting the code. Possibly the same cause as
batch 83's `func_08A9DD14` (not re-checked).

Found: 2026-10-15 · src/game/unsorted/hot_batch_86.cpp (func_0880BEF8)

## A fast gate on a loaded machine "never compares" what the routes reach on a quiet one: read `uptime` before reading `never compared`

**Symptom.** Batch 87's first fast gate: 48 functions installed, 17 compared, 31 `never compared` (`calls 0`), although the batch-78 probe had reached every one of them 1-5 times on the same routes.
**Cause.** The routes are wall-clock bound (`--max-seconds`, uncapped but not unlimited): the host ran at a load average of 57 on 24 cores (an Android emulator at 1,000 % CPU beside another agent's GPU gate), so each route
reached fewer game states in the same seconds and never got to the late, rarely called functions. Nothing in the gate's output says so.
**Fix / workaround.** Check `uptime` first; when the machine is loaded run the same gate with the route times doubled (`ROUTES["fast"]` scaled in a wrapper script, race 400 / dealer 200 / garage 206 / buy 220 / options 460):
43 of 60 compared then (every function written for the batch installed), 47 of 47 after the parents were left out (next entry; one more was unregistered afterwards, so the final batch registers 46), and the sabotage needed the same doubling. Re-run at the usual times once the load is gone. A heavy job of another agent
holding `gt-heavy.lock` also delays `tools.recomp` (it takes both slots) by the length of that job: the gate waits silently (it showed no output for 40 minutes).

Found: 2026-10-16 · src/game/unsorted/hot_batch_87.cpp (the whole batch)

## Descendants swallowed by a registered parent can be registered NESTED instead of being left behind: the gate then counts them in the parent's equal cases

**Symptom.** Batch 87's first complete gate: nine destructors (`func_0886DE00`, `func_0887ED48`, `func_0889A4F0`, `func_088A11F8`, `func_088AF1D0`, `func_08BD67B8`, `func_08ABF590`, `func_08B9355C`, `func_088A23EC`) compared 0 although
the probe reaches each once per menu route. Two functions of the batch, `func_0880D564` and `func_0880D478`, destroy the same member object (a virtual slot-0 call): their cases swallow everything below, as in the batch 86 entry.
**Cause.** Same as that entry (`call_guest` into a registered function that is not `GT_DECOMP_NESTED` runs and counts nothing); here the chain is two levels deep, so leaving the two parents out made four of the nine compared
but the other five were still swallowed, now by those four.
**Fix / workaround.** The two parents stay written but unregistered; the five that only run inside their parents' cases are registered `GT_DECOMP_NESTED` (`tools.contract_audit` 0): the gate reports them as `nested in 4 equal / 0 mismatched cases`
and `sabotage` pins them, but not within its 4 rounds: 43 of 46 were detected, and the last three (all nested) were pinned by a second run that sabotaged only them with the other 43 installed (`--also`).

Found: 2026-10-16 · src/game/unsorted/hot_batch_87.cpp (func_0880D564, func_0880D478)

## A wrapper whose memory effects live in its callees shows "cases that changed memory 0" from the sample, not from the code: run it dense before unregistering

**Symptom.** Batch 88's `func_08BFA640` (the recursion of a stable sort: it only calls `func_08BFA07C` on both halves and tail-calls the merge `func_08BFA3E0`) came out of two fast gates with `compared 23, equal 23, cases that changed memory 0`
(58 calls on the routes), which the rule "a function that writes memory needs a compared case that changed memory" reads as unverified.
**Cause.** The compared calls are a geometric sample of the 58 (23 of them); the few calls whose merge actually moves elements were not in it. The function writes nothing itself, but a wrong argument of its tail call only shows when the merged data changes.
**Fix / workaround.** `GT_DECOMP_DENSE=0x08BFA640 .venv/bin/python -m tools.decomp_gate fast TAG --only ...` (the tool inherits it) compared all 58 calls: 3 changed memory, 0 mismatches, and the function stayed registered. Before unregistering a function
for "no case changed memory", check `calls` against `compared`: when they differ, run it dense first (the nine unregistered getters/setters of batch 88 had `calls` = `compared`, so dense could not help them).

Found: 2026-10-16 · src/game/unsorted/hot_batch_88.cpp (func_08BFA640)

## `tools.decomp_gate` writes to a redirected log in 8 KB blocks: a sabotage run's round summaries can show up long after the round

**Symptom.** `nohup .venv/bin/python -m tools.decomp_gate sabotage ... > out &` (batch 88: three rounds of ~7 minutes) left the output file empty for the first ~10 minutes and then showed the mismatch lines of a round whose summary line
(`sabotage round N: ...`) was still missing, although the per-route logs under `recomp/analysis/gt_diff_<TAG>*_<route>.log` were filling.
**Cause.** Python block-buffers stdout when it is not a terminal; the tool's `print`s (route lines, mismatch lines, round summaries) reach the file only when 8 KB have accumulated or the process exits.
**Fix / workaround.** Follow a long run through its route logs (`ls recomp/analysis/gt_diff_<TAG>*`, `.venv/bin/python -m tools.decomp_gate report <TAG>`), or start it with `python -u`; the summary is complete once the process ends.

Found: 2026-10-16 · tools/decomp_gate.py

## Mode 1 (constant mip level) was sampled with the automatic level on decoded textures: GT draws millions of race draws that way

**Symptom.** The GPU gate's dealer route failed 413 of 588 frame pairs (most menus with 3D cars); distant objects in the race picked smaller mip levels than the console shows.
**Cause.** A histogram of the draws' texture level mode (4 minutes of the race route) gave ~8.2 M draws in mode 1 (constant level) with offset 0, ~5.5 M of them with mipmaps on. Patch 0039 turned mode 1 into a
fixed level (`textureLod`) only for render targets sampled as textures; a decoded texture kept the automatic level plus the offset as a bias. On the PSP (and in PPSSPP and our DX12 backend) mode 1 is that level, with no
slope term: level 0 for these draws.
**Fix / workaround.** lcs-recomp patch 0043: mode 1 returns 256 + the selected level for every texture. GPU gate x1: dealer 591/591 (was 175/588); race and garage unchanged.
Not the cause of the white streaks on distant signboards (they remain with the level forced to 0): see the culling entry below.

Found: 2026-10-15 · tools/patches/lcs-recomp/0043-constant-mip-level-for-textures.patch

## The line along the bottom of the screen: a full-screen picture's next texture row is its palette

**Symptom.** At an internal scale a 1-2 px line of changing colour along the bottom edge of the black notice screens, the intro video and menus (x4: the last two rows at 24 and 72 grey on a black screen; x1 clean).
**Cause.** GT draws these pictures as a transformed 4-vertex strip from the top 480x272 of a 512x512 CLUT8 texture (`0x096AE050`, CLUT at `0x096D0060`, 16 bytes after row 272). At 1x every pixel centre samples a
texel centre, so the bilinear filter never reads row 272; at x4 the pixel centres of the last PSP row sit at v = 271.625/271.875 and blend 12.5 %/37.5 % of row 272, i.e. the palette bytes read as indices.
PPSSPP has no special case for this (its 272-row logic only limits texture hashing). Not a present-pass problem (0035 already clamps that), and not the sprite path: the draw is hardware-transformed.
**Fix / workaround.** lcs-recomp patch 0044: each batch carries the range of the texture coordinates its vertices span (affine ones only), and ge.frag clamps the sample to half a texel inside it; a side that reaches the
texture edge stays open so repeating textures still wrap. It applies only at an internal scale: at 1x a magnified draw's edge pixels sample less than half a texel inside its range and
the PSP blends its neighbours there, so 1x stays exact. x4 dumps of the dealer route: the last rows equal the rows above in every frame.

Found: 2026-10-15 · tools/patches/lcs-recomp/0044-texture-sample-inside-draw-uv-range.patch

## White streaks on distant black signboards: the hardware path never culled back faces

**Symptom.** At an internal scale GT's distant signboards (the black "GRAN TURISMO" boards along the first race) showed thin vertical white dashes on the black, and the lower panel was an opaque white
board where the software renderer shows it see-through. Forcing mip level 0 did not change it; forcing nearest filtering seemed to (a coincidence of distance: the dashes come and go with it).
**Cause.** Recolouring every texture by address left the dashes white, so no texture drew them. The hardware-transformed path (`build_gpu_hardware_transform`) only culls with `PSPRECOMP_GE_GPU_HW_CULL=1`,
off by default upstream, so the board's back face (white, in the same plane as the black front) was drawn and won the depth test along vertical lines of equal truncated depth. With the variable set, the
winding was also inverted: every front face was culled (the race showed the insides of the objects). The CPU path (`prepare_screen_triangle`) culls correctly.
**Fix / workaround.** lcs-recomp patch 0045: culling on by default (`PSPRECOMP_GE_GPU_HW_CULL=0` turns it off) with `frontFace` swapped to the winding the GE accepts. x4 dumps of the race: the
board's black is clean and the lower panel is see-through as in the software frame.

Found: 2026-10-15 · tools/patches/lcs-recomp/0045-hardware-back-face-culling.patch

## The 10-70 instruction pool of the batch-78 probe ran dry at batch 89: a probe over 71-120 instructions found two families of 56 functions that the first one could not list

**Symptom.** Batch 89's selection (the batch-78 probe of reached functions, 10-70 instructions, no VFPU, no import-stub calls) left 25 candidates: of the 1,177 functions that probe reached, 48 of 10-70 instructions were
still absent from `src/`, and 9 had VFPU and 14 called stubs from 0x08C70000. So the pool could not supply another batch of about fifty.
**Cause.** Not a tooling fault: batches 78-88 had taken the pool, and the probe only ever listed functions of up to 70 instructions (`tools.family_members --write-probe-symbols PATH --max-insns` defaults to 70).
**Fix / workaround.** A new probe (`tools.decomp_gate probe b89p`) over the 1,403 not-yet-done functions of 71-120 instructions (a scratch script made the symbol list with the same filter; the tool's `--max-insns 120` was not
run) reached 281, 264 without VFPU, stubs or register jumps, and their masked-shape clusters (`tools.shapes` signature) held two families of the SAME 75-instruction function in two register allocations (46 + 10 members, one
call per route, a table-driven registration function): one template with the table in each row, and every member compared once per route in the fast gate (`cases that changed memory 1`), but 61 of the 67 functions of the
batch are `compared 0` in the real-time full gate (all 56 members among them), so their proof is the uncapped fast gate and the sabotage. Next time probe 71-120 (or 1-120) from the start and cluster before reading by hand.

Found: 2026-10-16 · src/game/unsorted/hot_batch_89.cpp (register_table)

## A hot function that never writes in its sampled cases may never write at all: the fast gate's DENSE stays at 32 cases per route, so run the binary by hand with `--decomp-cases` before judging

**Symptom.** Batch 90's `func_08BA9498` (a vector erase, 86,623 calls over the five routes of the fast gate) and `func_08C070FC` (a vector assignment, 74 calls) came out of the fast gate with `cases that changed memory 0` (160 and 24 compared cases,
all equal). `GT_DECOMP_DENSE=0x08BA9498 .venv/bin/python -m tools.decomp_gate fast ...` (the tool inherits it, FINDINGS batch 88) still compared only 32 cases per route (160 in all) and none wrote, so it could not tell "never writes" from "the sample missed it".
**Cause.** `route_shell` runs `build/gt_native --decomp diff` with the default case budget (32 per function per route) and has no way to pass `--decomp-cases`: DENSE only chooses which calls fill that budget, which is enough for a function with 58 calls (batch 88) and not for one with tens of thousands.
**Fix / workaround.** Run the binary by hand with the same environment and a larger budget, inside the slice:
`GT_DECOMP_ONLY=0xADDR GT_DECOMP_DENSE=0xADDR LCS_UNCAPPED=1 GT_PAD="$(grep -v '^#' config/pad_routes/race.pad)" systemd-run --user --scope --slice=gt-heavy.slice -- build/gt_native --max-seconds 200 --decomp diff --decomp-cases 3000`
(about 5 minutes per route). The race route compared 3,000 cases of `func_08BA9498` (12,231 calls) and every call of `func_08C070FC` (58 on race, 4 on dealer, 4 on garage); none changed memory, so both stay written but unregistered (a registered function needs a compared case that writes).
Also: `tools.decomp_gate coverage` prints the line numbers of the file as the build compiled it; comments edited while the run is going shift them, so map each report line by function (or by the offset of the lines added above) before editing the comments.

Found: 2026-10-16 · src/game/unsorted/hot_batch_90.cpp (func_08BA9498, func_08C070FC)

## A typo in a constant on an unexercised path is invisible to the gate: `tools.literal_check` compares every literal of a batch with the function's own disassembly

**Symptom.** Batch 90's review found `func_08C4A6B0` written with the limit 0x11111111 where the asm builds 0x01111111 (`lui 0x111` / `ori 0x1111`) on a path no route runs; the fast gate, the sabotage and the coverage run were all green.
**Cause.** The gate proves what its cases execute; the coverage report lists the untaken paths but nothing forces the constants on them to be re-read, and `tools.family_members --check` reads only one-line wrapper rows (template arguments and arguments of an inner call are not rows).
**Fix / workaround.** `.venv/bin/python -m tools.literal_check src/game/unsorted/hot_batch_N.cpp` (batch 91): each `void func_X(` body's hexadecimal literals (comments stripped, values below 5 skipped) must be a value of that function's `tools.disasm` output: immediates, displacements, resolved addresses, call targets, the 32-bit value of a `lui`/`ori` pair
or of a bare `lui`. Batch 91: 52 functions, 0 missing with one manual `--allow` (a displacement 8 + 0x34 of a store through `s1 + 0x34`); two literals mutated by hand (`0x3E8E38E4` to `...E5`, `0x08C99738` to `...39`) are reported. It does not check which register or which call a value goes to: that is still the reading.
Also seen in batch 91: a function can be sampled with `calls 0` on all five routes of the fast gate although the batch-89 probe counted hits for it (`func_08B666E8`, 12 hits): a probe hit is not a promise that the dispatcher sees the call.

Found: 2026-10-16 · src/game/unsorted/hot_batch_91.cpp, tools/literal_check.py

## The delay slot of a `jal` sees the register BEFORE the call: `addu s1, v0, zero` there is the previous result

**Symptom.** Batch 91's first fast gate: `func_0899CE20` mismatched on all 11 of its cases with `Guest memory access outside PSP RAM/EDRAM at 0x00000005`, not at its own code.
**Cause.** `jal func_08A5BA08` has `addu $s1, $v0, $zero` in its delay slot (it runs before the call), so s1 is the word the function loaded into v0 earlier, not what the call returns; written as `s1 = call_set(...)` the function stored the callee's result at `u[0x20]` and a later callee dereferenced it.
**Fix / workaround.** Set the register before the call (`ctx.gpr[17] = pending;`). It was visible only because the stored word was used by a callee in the same case; a stored word nothing reads in the case would have passed. Re-read every `addu rX, v0, zero` that sits in a delay slot, the `bnel`/`beql`/`bgtzl` ones (executed only when taken) included.

Found: 2026-10-16 · src/game/unsorted/hot_batch_91.cpp (func_0899CE20)

## The side-to-side wobble: GT jitters its 3D projection by half a pixel on alternate frames

**Symptom.** On the laptop (a 15 s screen recording, 60 fps, time trial with the car stopped) the whole 3D scene jumped back and forth between two positions frame after frame (±1-2 px horizontally and 2 px vertically at 1080p) while the HUD stayed still.
**Cause.** Not the presentation and not the viewport (both framebuffers use the same viewports and the same present size). Logging the projection of the first perspective draw of each list on the race route: the lists drawing into the framebuffer at VRAM 0 carry `P[8] = 1/480 = 0.0020833` and `P[9] = 1/272 = 0.0036764`, the ones drawing at 0x44000 carry 0 — half a pixel in both axes, every other frame. GT's temporal anti-aliasing: the PSP's slow LCD blended the two pictures into one. The software renderer draws it too (it is in the game's matrices).
**Fix / workaround.** lcs-recomp patch 0048: the hardware transform and the CPU vertex decoders drop exactly that offset (`projection_without_temporal_jitter`, only `|P[8]| = 1/480`, `|P[9]| = 1/272` on a perspective projection) unless `[Rendering] KeepTemporalJitter=true` (`gt_native` always keeps it, so the differential gate is unaffected); `tools/gpu_gate.sh` keeps it, to stay comparable with the software reference (GPU gate x1 unchanged: race 344/943, dealer 488/488, garage 476/476). x4 dumps of the start grid (camera almost still): consecutive-frame differences alternate high/low with the jitter (5.1 2.8 2.7 6.3 2.9 4.7 …) and grow smoothly without it (1.8 1.8 1.9 1.9 …).

Found: 2026-10-16 · tools/patches/lcs-recomp/0048-drop-temporal-jitter.patch

## The 71-120 pool ran dry at batch 93: a probe over 121-170 instructions reached 192 more functions, and `tools.shapes --skeleton` joined the register allocations the masked shape kept apart

**Symptom.** After batch 92 the batch-89 probe had 80 reached functions left without an active registration; the ones not yet written are VFPU, call the import stubs or jump through a register, are float-heavy, write no memory of their own or are tangled (the head comments of batches 89-92 say which, function by function), so only `func_08B0ACDC` could be written. `tools.family_members --list --any-template` over a probe of the next size class found one family (3 members).
**Cause.** Not a tooling fault: batches 89-92 took the pool (the same thing the batch-89 entry says for 10-70). The masked shape (`tools.shapes`) keeps registers, so one function compiled into two register allocations is two clusters.
**Fix / workaround.** `tools.family_members --write-probe-symbols FILE --max-insns 170` (filtered to 121-170: 557 not-done functions) + `tools.decomp_gate probe b93p --symbols FILE` reached 192 (all five routes, about 10 minutes of routes). Comparing instruction kinds only (new `tools.shapes --skeleton`, 3 tests: registers, immediates and branch offsets ignored; R-type function field, REGIMM rt and COP1 format kept) found seven families of two or more reached functions
(`tools.family_members --list --any-template` had found one): 26 of the batch's 44 registered functions are instances of nine templates (the same 154-instruction parser four times, a 134-instruction flag handler in two shapes with two more variants each, a counted-holder assignment twice, and a vector fill-insert twelve times with different element sizes and allocators, sharing `fill_insert_grow`). Next time probe 121-170 (or everything up to 170) from the start and cluster with `--skeleton` before reading by hand.

Found: 2026-10-16 · src/game/unsorted/hot_batch_93.cpp, tools/shapes.py

## A callee reads an argument register the C++ never passes: it sees the value the C++ context holds, not the one the original had

**Symptom.** Batch 93's first fast gate: the four members of `scan_block` faulted on every case (`Guest memory access outside PSP RAM/EDRAM at 0x0000000E`, 0x11, 0xD) and `func_08896780` on 0x105. The faults were not in the function: the virtual slot called after the loop's `bne` read `a1` = the stream (set in the `bne`'s delay slot, `addu a1, s2`), and `func_0892BB10` / `func_0892B968` return `[a0 + 0x260] + k` with `a0` = `this[0x6C]`, loaded 30 instructions earlier and never written again.
**Cause.** `call_set(rt, ctx, addr, {a0})` passes what the call site's own instructions set; a register the original left set by an earlier instruction (a delay slot, a load, an earlier call) is whatever the C++ `ctx` holds, which is the value of a different earlier `call_set`. Only the paths the routes take show it.
**Fix / workaround.** For every call that passes fewer than four arguments (and every virtual call), read the head of the callee (`tools.disasm CALLEE | head -20`) for reads of `a1`-`a3`/`t0`-`t3` before they are written, and pass them. A scratch script that compared the number of arguments of each literal call with the callee's reads found one more on a path no route runs (`func_08BF9B6C` takes the end of the range as `a2`; the caller of a never-compared native passed two arguments) and flagged two false positives that were correct (a callee that spills `a1`-`t3`
to the stack).
Also seen: a function that ends with a void callee returns the `v0` the original had at that call, which can be an unrelated `sll v0, s3, 2` between the previous call and it (`func_08BFB68C` returned the destination of the previous call, the original `tail << 2`): the first gate caught it as `v0: original 0x0, decompiled 0x094AD1D4`.

Found: 2026-10-16 · src/game/unsorted/hot_batch_93.cpp (scan_block, func_08896780, args_to_vector, fill_insert_scalar)

## A disassembly view with the callee-saved reloads hidden also hides a real `lw $s0, 4($sp)`: `tools.sp_loads` lists the ones to read

**Symptom.** Batch 94's first fast gate with `func_08A1F328`: both compared cases of each menu route ended `decompiled run faulted: Guest memory access outside PSP RAM/EDRAM at 0x13990043` (0x1398F983 in the next run), not inside the function's own code; the other 34 functions of the batch passed the first time.
**Cause.** The functions were read through a scratch view of `tools.disasm` that dropped every `lw $sN/$fp/$ra, off($sp)` and `sw` of them to skip the prologue and the epilogues. At 0x08A1F3E0 `lw $s0, 4($sp)` is not a restore: `func_08A1EC18(.., sp + 4, ..)` stores a new object there and the next calls (func_08AF771C, func_08AF7B0C, func_08A44D9C) work on it, while the C++ kept the argument object in `s0`.
**Fix / workaround.** `tools.sp_loads` (new, 3 tests) lists the `lw $sN/$fp/$ra, off($sp)` of each function of a batch file whose register was not saved to that offset; on the 35 functions of batch 94 it found this one and 11 loads the C++ already models (the string data pointers re-read after a call in `parse_floats` and `func_089AF6A0`). Run it with `literal_check` before the first gate. A gate would not have found a hidden load on a path no route runs.

Found: 2026-10-16 · src/game/unsorted/hot_batch_94.cpp (func_08A1F328), tools/sp_loads.py

## The GE's per-draw cost was mostly redone state: a revision and a quad-strip fast path cut it by half

**Symptom.** On two cores the GE thread spent ~730 ns per draw (~420 ms/s in a race), and the game thread waited for it 28 % of the time; on the laptop, 30-50 frames/s skipped and audio dips.
**Cause.** GT's executor passed no state revision, so the renderer's revision-keyed caches never hit: every draw rebuilt the 352-byte descriptor, the lighting, the hardware transform, ran 3-4 texture-map lookups and 7-9 backend locks — while before 97 % of race draws only the texture level 0xC8 (or nothing) changed, and 94 % of the draws are textured quad strips of at most 8 vertices.
**Fix / workaround.** Two steps (spec `docs/superpowers/specs/2026-10-16-ge-fast-path-design.md`): `host/ge_state_revision.cpp` + lcs-recomp patch 0046 (the caches hit; ns per draw 730 → 574), and patch 0049, a fast path for the quad strips on a revision hit (→ 372 ns, lists 252 ms/s, GE wait 12 %). Both have byte-for-byte verify modes (`GT_GE_REVISION_VERIFY`, `GT_GE_FASTPATH_VERIFY`) and sabotage runs that the verify caught. Pitfalls met on the way: struct copies do not keep padding (compare member-wise, build into zeroed bytes); a matrix data register is a stream (bump on every write); the content signature is set only on the draws that re-check a texture, so the verify compares the texture keys it produces, not the signature itself; A/B runs of the same route drift to different scenes, so compare ns per draw, not ms/s. What the fast path assumes (review of #225): a texture's content does not change during a list without a noted write (GT rewrites its glyphs through 0xEA, which notes the write and bumps the revision; a CPU write to a texture a running list samples would need a GE sync), and it only runs where the software raster is skipped unconditionally. Fast draws are not counted in `GT_GPU_STATS`'s per-draw report (draw_calls, blend stats); `game_draw_calls` and the perf log's ns per draw do count them. The verify ran on the race, dealer and garage routes only.

Found: 2026-10-16 · tools/patches/lcs-recomp/0046-draw-state-revision-caches.patch, 0049-ge-fast-path-quad-strips.patch

## A backend entry of its own for the fast path was slower: measured and dropped

**Symptom.** After 0049 the backend's general accumulate call was still the top function of the GE thread (11.5 % of samples, two cores, race). A dedicated entry for the fast draws (one lock, strip indices written directly, the render-target lookup once per revision, merging by revision instead of comparing 352 + 180 bytes) measured slower: 300-334 vs 286-290 ns per draw, two interleaved runs each.
**Cause.** Merging by revision alone made more batches (the general path's first draw of each revision could not be joined: recording +11 %); with the general merge test as fallback it was still no faster: the compares and the lock it removed are not where the time goes.
**Fix / workaround.** Not merged (2026-10-16). Measure first: `tools.thread_cpu --name gt_play` shows the CPU per thread (the GE thread is named `gt-ge`; the game thread keeps the process name): on two desktop cores in a race the GE thread is ~47 % and the game thread ~35 % of a core.

Found: 2026-10-16 · tools/thread_cpu.py

## A string release that finds the shared empty cell leaves the `v0` the asm held, and a `lui v0` / `addiu s1, v0` pair leaves only the high half there

**Symptom.** Batch 95: eight functions end, or run to a point with no call after it, with the string-cell release sequence (`beq data - 0xC, D_08D74FD0` skips `func_08B60804` / `func_08B8542C`). When it skips nothing is called, so the function returns the `v0` the instructions before it left, and `v0` is compared (`ReturnKind::Word`). Nothing in `tools.disasm` says which constant that is.
**Cause.** The asm builds the shared cell's address in `v0` before the compare: `lui v0, 0xA` then either `addiu v0, v0, -0x7530` (v0 = 0x08D74FD0: `func_08A233B0`, `func_08A31244`, `func_08A26744`, `func_08A26940`) or `addiu s1, v0, -0x7530` (`func_08A6E974`: v0 keeps only the high half). `tools.disasm` prints the pair with its resolved address ([HI] / [LO] = 0x08D74FD0), so the half is invisible; the recompiled unit shows it (`recomp/generated`, label L_08A6EA24: `ctx.gpr[2] = (2263u << 16u)`, 0x08D70000). The vector writers (`func_08BEFA18` and its two siblings) leave 0x08CCD990 in `v0` after the single element for the same reason.
**Fix / workaround.** Set `ctx.gpr[2]` to what the asm holds before `release_cell` (a call inside it overwrites it, so only the skip path keeps it) and read the constant from the generated unit (`grep -n "^L_<address>:"`), not from the annotated pair; `tools.literal_check` reports 0x8D70000 as missing, so it is an `--allow` entry. The gate confirmed the constants: `release_cell`'s skip and release arms both ran in the compared cases of the batch with `v0` equal.

Found: 2026-10-16 · src/game/unsorted/hot_batch_95.cpp (func_08A6E974, func_08A233B0, release_cell)

## The 121-170 pool ran dry at batch 95: 27 of the 111 left were writable, and a 171-230 probe reached 104 functions and one family of five

**Symptom.** After batch 94 the b93p probe still had 111 reached functions without a `func_X` in `src/`; `tools.shapes --skeleton` finds no cluster among them and what is left is jump tables, recursion, tangled likely-branch code, float code, imports that block and units made of several tail-called blocks. Batch 95 wrote 31 of them (27 registered) by reading, one by one.
**Cause.** Not a tooling fault: batches 89-94 took the cheap part of the pool (the same thing the batch-89 and batch-93 entries say for 10-70 and 71-120).
**Fix / workaround.** `tools.family_members --write-probe-symbols FILE --max-insns 230`, filtered to 171-230 (348 not-done functions), `tools.decomp_gate probe b95p --symbols FILE` reached 104; `tools.shapes --skeleton` found one family with reached members: five copies of a 182-instruction hinted insertion into a string-keyed red-black tree, equal except for two callees (their own unique-position search and node insertion), so one body and five table rows, `tools.family_members --check` reading the rows. Its (word, string)-keyed sibling (`func_08C34C58`, 45,104 calls) shares no skeleton and was read separately. Next time probe 231-300 from the start, or everything above 170, and cluster with `--skeleton` before reading by hand.
Also new: `tools.arg_reads` (5 tests) lists the calls that pass fewer argument registers than their callee reads (the batch-93 stale-argument bug, found by a scratch script then): six in this batch, all harmless; the scan is by address order, so each line is a place to read the callee's head.

Found: 2026-10-16 · src/game/unsorted/hot_batch_95.cpp (hinted_insert), tools/arg_reads.py

## A `j` forwarder hides the argument registers of its target: `tools.arg_reads` read only the stub and printed nothing

**Symptom.** Batch 96's `func_08AD6EBC` came out of its first fast gate with `compared 98, equal 0, mismatched 98` (`RAM at 0x093EDF00: original 00, decompiled 55 (60 bytes differ)`), although the C++ matched the asm line by line and `tools.arg_reads` was clean. The 60 bytes were the first elements of the array the function fills (`0xFFF85555` over them).
**Cause.** Its last call, `func_08AD6A00(begin, end)`, is a two-instruction forwarder (`j func_08C3E4E8`) of a partial sort `(first, middle, last)`; the original still held `last` (the cursor) in a2 when it made the call, the C++ passed two arguments and the sort saw whatever a2 held in the context (a stack address), writing its heap bookkeeping into the vector. `tools.arg_reads` scanned only the forwarder's own two instructions, which read no register.
**Fix.** The call passes `{begin, end, cursor}`; `tools.arg_reads` now follows a callee of at most 8 instructions that ends in `j target` to the target (minus the registers the forwarder writes first; 3 tests) and flags the old two-argument call. The other forwarders of the file (`func_08A06E94`, `func_08A06EC8`, `func_08AD5EC8`, `func_08AD5ED0`, `func_08B344E0`, `func_08B4E9D4`) read nothing past the arguments the calls pass. A function whose first gate mismatches on every case and whose C++ looks right: read the callees' callees.

Found: 2026-10-16 · src/game/unsorted/hot_batch_96.cpp (func_08AD6EBC), tools/arg_reads.py

## The v0 a function returns can be the constant of a test, not of a computation: `andi v0, v1, 0x20` before a last `jal`, `addiu v0, zero, 2` in a loop bound

**Symptom.** Two of the 24 functions of batch 96's second fast gate mismatched on v0 only: `func_089230A4` (original 0, decompiled the previous call's v0) and `func_0895C140` (original 2, decompiled a stack address), both registered as Word.
**Cause.** Neither v0 is a result. `func_089230A4` ends with `jal func_08B4E9D4`, a forwarder that sets no v0, so its v0 is whatever the instruction before the last branch left: `andi v0, v1, 0x20` (zero, because the branch around the 0x20 block was taken). `func_0895C140` ends with a loop that tests `bne s2, v0, ...` after `addiu v0, zero, 2`, so every exit leaves v0 = 2. The C++ left the v0 of the last guest call.
**Fix.** The C++ sets `ctx.gpr[2]` to the constant the test leaves (`= 0` in the not-taken arm, `= 2` after the loop). A Void registration would not have seen it; a Word one saw it on the first gate. For every Word function read the last three instructions of each exit for a `slti/sltiu/andi/xori/addiu v0` before the last `jal` or the final `bne`.

Found: 2026-10-16 · src/game/unsorted/hot_batch_96.cpp (func_089230A4, func_0895C140)

## `--decomp on` froze at ~20 s of the race route: call_guest could not resume mid-function, and a stopped runtime answered 0 forever

**Symptom.** With every decompiled function installed (`gt_native --decomp on`, race pad route), the run stopped printing `[perf]` lines after ~20 s and never reached its `--max-seconds`; the main thread sat at 100 % inside `func_08A63F1C`'s loop (batch 83), which calls the script interpreter step `func_08A63C5C` until the stack shrinks. Bisecting with `GT_DECOMP_ONLY` (13 runs) isolated `func_08A63F1C` alone; its C++ matches the asm, and the gate passed it (only 10 of its 624 calls are comparable: it is reentrant, and 2 block on an LwMutex).
**Cause.** Two faults in `gt::call_guest`, the loop that runs guest code for decompiled callers. (1) A callee that blocked (LwMutex contention in `func_08ACB1BC`) resumes at its return address `0x08A1CFD4`, mid-function: the dispatcher finds that pc through the generated unit that holds it, `call_guest` only through `function_at` (function starts), so it stopped the runtime with "no function at 0x08A1CFD4". It also ran no scheduler boundary, so virtual time never advanced and a guest loop polling another PSP thread (`jal func_08ACB1BC; bnez v0` → 50 M steps, "or no return") never saw it progress. (2) After the stop every `call_guest` returned 0 at once; a decompiled loop that waits for its callee's progress spun forever, and the stop reason (printed only when the dispatcher returns) never appeared — an error disguised as a hang.
**Fix.** lcs-recomp patch 0050 exposes `Runtime::dispatch_function_at` (unit first, as the dispatcher) and `Runtime::dispatch_boundary` (the per-dispatch scheduler boundary); `call_guest` uses both. A `call_guest` on an already stopped runtime throws `gt::GuestStopped`, which the decomp trampoline and `fiber_main` absorb, so the dispatcher ends the run with the real reason. Tests: `guest_call_tests` (a pc inside a generated unit, the boundary per dispatch, the throw) and `fiber_tests` (the fiber absorbs it), each seen failing first. The race route with all functions then runs its 60 s and stops on the wall clock.
**Open (resolved 2026-10-19: see the two entries at the end of this file).** With `--decomp on` the import trace leaves `--decomp off`'s at import 4,406 (two `off` runs are identical over 200,000 imports) and the route no longer reaches the race in 60 s. Not shown to be a decompilation fault: `call_guest` counts its steps as dispatches while the recompiled code chains calls inside a unit without counting them, so preemption lands elsewhere and the thread order changes. The differential gate (`--decomp diff`) is unaffected; `--decomp on` fidelity is its own question.

Found: 2026-10-16 · host/guest_call.cpp, host/fiber.cpp, host/diff_harness.cpp, tools/patches/lcs-recomp/0050-dispatch-entry-points-for-host-loops.patch

## A decompiled function that calls `sceRtcGetCurrentClockLocalTime` itself was "not comparable" in every case until the import was recordable

**Symptom.** Batch 97's `func_08812CEC` (a clock window that calls the import stub `0x08C7B018` on its own stack buffer) came out of the first fast gate with `compared 0, not comparable 17` and `first not comparable: original: import stub 0x08C7B018 (sceRtc::0xE7C27D1B)`.
**Cause.** The harness runs an import twice only when it is replay-safe (`kReplaySafeNids`) and records it once otherwise (`kRecordableNids`); a third kind is refused inside a case. It was in neither list. (`rtc_now_ticks` is a fixed start date plus the virtual time, deterministic and read-only, so replay-safe would have worked too, like `GetSystemTimeLow`; recording is also valid.)
**Fix.** `{0xE7C27D1B, 0x01}` in `kRecordableNids` (host/main.cpp): reads a0, writes the 16-byte date through `GuestMemory`, never switches threads. `func_08812CEC` then compared 17 of 17 equal. The other imports that block (`sceKernelWaitSema` 0x08C7AA20, `sceKernelWaitEventFlag` 0x08C7AA10) are still refused: `func_08AF0B78` (a ring-buffer read that waits on a semaphore every call) is not comparable in 89 of 89 cases and `func_08AEB4CC` in 88 of 99, so both stay unregistered.

Found: 2026-10-16 · host/main.cpp, src/game/unsorted/hot_batch_97.cpp (func_08812CEC, func_08AF0B78, func_08AEB4CC)

## A once-only initialiser that runs inside another decompiled function's case shows `calls 0`: register it `GT_DECOMP_NESTED`

**Symptom.** `func_08B610C0` (called once, from a static-constructor chain) was compared 1 of 1 equal when it was the only installed function (`GT_DECOMP_ONLY=0x08B610C0`) and `calls 0` as soon as `func_08B5BB0C` (the function whose static initialisation reaches it) was installed too, in all five routes.
**Cause.** With `func_08B5BB0C` installed the call happens inside one of its cases; a nested call is not counted in `calls`/`compared` unless the callee is registered nested, in which case it runs decompiled in the decompiled run and is counted in `nested in N equal cases`.
**Fix.** `HOT97N` (= `GT_DECOMP_NESTED`) for `func_08B610C0`: the run reports `nested in 1 equal / 0 mismatched cases`. The return contract is fine for nesting: `tools.contract_audit` lists no caller that reads a register the function leaves differently (it sets v0 / v1 like the original). Before unregistering a function for `calls 0`, install it alone once: `GT_DECOMP_ONLY` shows whether another installed function swallows its calls.

Found: 2026-10-16 · src/game/unsorted/hot_batch_97.cpp (func_08B610C0)

## A function whose tail callee reads uninitialised stack cannot be proven: the new elements of a vector were filled with the dead stack of earlier calls

**Symptom.** Batch 97's `func_08B04D98` (builds an object of seven vectors and tail-calls `func_08B039F8` → `func_08B05DC4`) mismatched on all 11 compared cases of every route, `RAM at 0x093FDC08: original A0, decompiled 00 (3876 bytes differ)`, with or without the tail call replaced by a `call_set`. Every store, call and argument of the C++ matches the asm; the allocator's bookkeeping words (`0x08D69338`) also differ, and the differing bytes are records of 0x70 bytes that contain the `out` pointer and return addresses (`0x08C4A630`).
**Cause (inferred).** `func_08B05DC4` grows the vector at `object + 0xC8` with `func_08C4C0A0(vector, position, count, sp + 0x20)`, where `sp + 0x20` is an element of the callee's own frame that nothing initialises: the new records are copies of whatever the earlier calls left there (saved registers, return addresses). The decompiled function does not reproduce dead-stack spills (`docs/agents/decomp-workflow.md`), so the two runs fill the records differently.
**Fix.** None: the function stays written and unregistered (commented `HOT97` line). A function whose callee builds elements from an uninitialised temporary has no deterministic result to compare; before reading such a function look for a `jal` whose last argument is `sp + N` in a callee that never stored it.

Found: 2026-10-16 · src/game/unsorted/hot_batch_97.cpp (func_08B04D98)

## A callee that reads a float argument register is not covered by `tools.arg_reads` or `tools.literal_check`: draft code passed the wrong f12 and a hex-less float constant (25.0f) as 100.0f

**Symptom.** Batch 98's first gate: `func_0894A15C` mismatched 16 of 20 cases (`RAM at 0x093E1D90: original DF, decompiled CD`, a float at A + 0x38) and `func_0880E6EC` 8 bytes of a window cell (`original 41, decompiled 42`). Both came from a draft that had never been gated.
**Cause.** (1) `func_08949B04(index; f12)` reads `f12` (a clamp of the float `A[0x30]` that the caller loaded into f12 earlier); the C++ called it with `call_set`, so the callee saw whatever f12 the context held. `call_guest` runs the callee on a copy of the context, so nothing carries over but v0/v1/f0. `tools.arg_reads` follows only the integer argument registers (a0-a3, t0-t3). (2) The cell width constant is `0x41C80000` = **25.0f**, written as `100.0f`; `tools.literal_check` checks only hexadecimal literals, and decimal float literals are invisible to it.
**Fix.** `call_setf(..., {f12})` for the callee and 25.0f. Rule: for every guest call in a float function read the callee's head for `$f12`-`$f19` uses and pass them; check each decimal float literal against the asm constant by hand (`0x41C80000` = 25.0f, `0x42C80000` = 100.0f, `0x41A80000` = 21.0f, `0x41B00000` = 22.0f).

Found: 2026-10-19 · src/game/unsorted/hot_batch_98.cpp (func_0894A15C, func_0880E6EC)

## Pool: reached functions of 22-120 instructions that no batch had registered (batch 98 probe `b98s`)

Probing every unregistered function of 20-120 instructions (7,866, `tools.decomp_gate probe b98s --symbols FILE`, 4 minutes) found 43 that the five routes call, among them hot float and table-search functions (`func_088C4BB0` 16,086 calls, `func_088C50D4` 23,102, `func_08977904` 46,257, `func_089C54B8` 60,047). The ones left out are VFPU (`func_08B53D2C` 7.2 million calls, `func_08B54D28`, `func_08B55218`, `func_08B565F8`, `func_089A900C`), reach `sce*` imports (`func_08ACB3B4`, `func_08B31AB4`, the `func_08B8443C` / `func_08B84564` family) or work on 64-bit values (`func_08B6728C`, `func_08B66E88`). Earlier probes covered only 121-230 and 231-300 instructions; probe the lower band again when the larger pools run dry.

Found: 2026-10-19 · batch 98

## "The address does not appear in src/" is the wrong filter for the pending pool: it hides every function that is only called by decompiled code (batch 99 found 463 reached functions of 20+ instructions that way)

**Symptom.** Batch 98's probe listed 43 reached unregistered functions of 20-120 instructions. Batch 99's probe (`b99p`: `tools.family_members --write-probe-symbols FILE --max-insns 420`, then `tools.decomp_gate probe b99p --symbols FILE`, 14,157 recompiled functions, about 6 minutes for five routes) found 668 reached, 463 of them of 20 or more instructions, many of them hot: `func_08B58874` (9.7 million calls over the five fast routes), `func_08B5596C` (8.1 million), `func_08B52578` (1.7 million), `func_089A932C` (0.9 million), `func_08B6D564`, `func_08C20B4C`, `func_089DA5C0`, `func_08B49D98`. None of them was registered, but several had been skipped by earlier batches only because their address already appeared in `src/` (as a `call_set` target or in a comment).
**Cause.** "grep the address in `src/` first" also matches the functions a decompiled caller merely CALLS; those still run recompiled. The test that means "already decompiled" is a `void func_XXXXXXXX(` definition or a registration line (`NAME(0xADDR, …`, commented ones included, they were read and left on purpose).
**Fix.** Filter with those two patterns (see `decomp-workflow.md` § Picking candidates). What is left after batch 99 (reached, 20 or more instructions, not written): about 430 functions; the hottest ones are VFPU code (`func_08B53D2C` 8.4 million calls, `func_08B54D28`, `func_08B55218`, `func_08B565F8`, `func_089A900C`), copy-on-write string code (`func_08B8634C` 1.4 million, `func_08B8A4C0`, `func_08AACAF8`, `func_08A32F9C`), soft-float / 64-bit code (`func_08B6EF68`, `func_08B66E88`, `func_08B6728C`), lock-and-import code (`func_08AEFFE0`, `func_08AEC210`, `func_08AF0508`, `func_08B14A18`, `func_08B2BFDC`, `func_08B2BF20`) and a 0x6C0-byte-frame decoder (`func_08B20DB0`).

Found: 2026-10-19 · batch 99

## A virtual call whose callee leaves `v0` alone hands the vtable (or the function address) on as the caller's return value

**Symptom.** Batch 99's second gate: `func_0886AAA4` mismatched `v0: original 0x08CBFA00, decompiled 0x09DB0450`.
**Cause.** The function ends with a loop of `lw v0, 0(s0); lw v1, 0x14(v0); jalr v1`: at the call `v0` holds the vtable and `v1` the slot's function. The slot's callee is a setter that returns without writing `v0`, so the original returns the vtable address. `call_guest` starts the callee from the context, so a `call_set` on the slot's function left whatever `v0` the earlier call had produced.
**Fix.** `call_virtual(object, offset, args)` sets `v1 = vtable`, `v0 = function` (the `lw v1, 0(obj); lw v0, off(v1); jalr v0` shape; other register shapes are set by hand: `jalr t2` leaves `v0 = vtable`) before the call. Rule: when an indirect call can be the last thing that writes `v0` (the function's return value, or a branch condition that tests it), give `v0`/`v1` the values the compiler's address computation left there.

Found: 2026-10-19 · src/game/unsorted/hot_batch_99.cpp (func_0886AAA4; applied to every virtual call of the batch)

## The laptop's slow GE was its power profile: power-saver caps the 5850U at 1.9 GHz

**Symptom.** On the laptop (Ryzen 7 PRO 5850U) the race skipped 7-9 frames/s, the GE thread ran at 78-83 % of a core (~690 ns per draw) and the game waited for it 29 % of the time. The GPU spent only ~2.1 ms per frame.
**Cause.** The `low-power` / `power-saver` platform profile caps the CPU at 1.9 GHz (`lscpu`: max 1901 MHz). In `performance` (max 4.5 GHz) the same build runs ~290 ns per draw, waits 5 % and skips 0.3 frames/s. The GPU was never the limit.
**Fix / workaround.** None in code: lcs-recomp patch 0051 cut the per-draw cost a further 3-5 % (spec 2026-10-19). When a laptop report says the GE is slow, check `cat /sys/firmware/acpi/platform_profile` and `lscpu | grep MHz` first. A race measured in power-saver and one in performance are different machines.

Found: 2026-10-19 · laptop measurements over SSH (docs/superpowers/specs/2026-10-19-ge-backend-per-draw-cost-design.md)

## A setjmp cannot be diff-tested: it saves the harness's sentinel as the return address, and the later longjmp jumps to it outside the case

**Symptom.** Batch 100's first fast gate stopped every route after 3 compared cases: `Unsupported Allegrex instruction 0x00000000 at 0x0BAD0004: diff harness sentinel reached without a pending continuation`.
**Cause.** `func_08B7B2E4` stores s0-s7, sp, fp, ra and f20-f31 in the caller's buffer. Inside a case `ra` is the case's sentinel, so the buffer keeps the sentinel; the game's later `longjmp` (`func_08B7B34C`) restores it and returns into a sentinel whose case is long over. Bisected by running the batch halves with `GT_DECOMP_ONLY` (one function alone: clean).
**Fix / workaround.** Leave setjmp-style functions recompiled (the HOT100 line is commented with this reason). A route-level stop with an "Unsupported instruction at 0x0BAD…" is a registered function that leaks the sentinel into guest memory: bisect the registrations, do not look at the arithmetic.

Found: 2026-10-19 · src/game/unsorted/hot_batch_100.cpp (func_08B7B2E4)

## Reached by the probe is not verifiable: of 40 written functions of the b100p pool only 11 had a compared case that changed memory

**Symptom.** Batch 100 wrote 40 reached functions (probe `b100p` over the 14,121 unregistered recompiled functions: 638 of 20 or more instructions reached) and could register 11. Ten reach `sce*` imports in every case (compared 0, all not comparable: the `func_08B100D0` family that builds a 0x3C-byte object, `func_089C5BB8`, `func_08ACEB40`, ...). Eighteen were compared (up to 3,000 cases with `GT_DECOMP_DENSE`) but only on their empty paths: the array fills `func_08BE6D44` / `func_08BE6674` / `func_08BE834C` (805 / 352 / 192 dense cases, no dest or count 0), the range copies `func_08C2B76C` (3,000 cases), `func_08C15E8C`, `func_08C4B3C0`, the vector assignments `func_08C07E24` / `func_08C070FC`, `func_08AF2D20`, `func_08872E18`.
**Cause.** The routes call the container helpers with empty containers (a probe hit count says a function runs, not that its body does).
**Fix / workaround.** Read the coverage output (`tools.decomp_gate coverage`) before registering: "regions never run" over the whole body means the compared cases prove nothing. More routes (a race with several cars, the replay and photo modes) are what these need, not more code.

Found: 2026-10-19 · batch 100

## After 100 batches the hottest unwritten reached functions are 2-instruction tail jumps and getters, and a size cut-off hid them

**Symptom.** Batches 99-100 took reached functions of 20 or more instructions and kept running into pools that only ran empty paths. Batch 101's probe (`b101p`, 13,968 unregistered recompiled functions of at most 300 instructions) ranked by calls shows the real leftovers: `func_08B4B864` (134,690 probed calls, `j func_08B4B7B4` with a delay-slot move), `func_08B77BF4` (97,882), `func_08B4BAA8` (86,154), `func_08C31304` / `func_08C3131C` (the same with a sign extension of `a1`), plus getters and setters. All of them compared in the first fast gate.
**Cause.** The earlier pools were cut by size (20+ instructions) and by "stores to memory", which drops forwarders and pure getters. A function with no stores of its own is fine for the gate: the rule is a memory-changing case only for functions that write memory.
**Fix / workaround.** Rank the probe by hits and a static filter instead of by size: a `jal` or `j` target at or past the end of `.text` (0x4767DC in ELF terms) is an import stub, and walking the callees three to five levels marks the wrappers that reach one. In batch 101 the walk marked three of the five functions that compared 0 (`func_08AC3308`, `func_08AD08A4`, `func_08AD0974`); the other two (`func_08ABC9D0`, `func_08ABCC1C`) end in `jr t9` through a vtable slot, which a static walk cannot see, so those still need the gate. Allocator wrappers (`func_0892BF7C` calls `func_08B6461C`) are marked too but compared fine: the lock imports are replay-safe. Where the tail stub's callee returns a pair, register it as `DoubleWord` (`tools.contract_audit` lists the callers that read `v1`: `func_08B6E268`, `func_08B77BF4`, `func_08BBC08C`).

Found: 2026-10-19 · batch 101

## `tools.family_members --check` calls two sibling wrappers BAD when one loads its constant with `addiu` and the other with `addu $zero, $zero`

**Symptom.** `func_0885E794` and `func_0885E7B8` differ only in a1 = 1 versus a1 = 0. Written as `void func_0885E7B8(...) { forward_flag_to_child(rt, ctx, 0u); }` the check printed `BAD func_0885E7B8: its words do not decode to the same sequence of values as the other rows of the wrapper` (the 0 comes from a register move, the 1 from an immediate).
**Cause.** A wrapper row is any function whose body is one call with literal arguments; the check decodes the argument values from the member's own immediates, and a move from `$zero` has none.
**Fix / workaround.** Set the register in the function (`ctx.gpr[5] = 0u;`) and let the shared helper read it, so the body is no longer a one-call row. Not a bug in the decompiled code: the gate compared both.

Found: 2026-10-19 · src/game/unsorted/hot_batch_101.cpp (func_0885E794 / func_0885E7B8)

## `--decomp on` does not always reach the race on the race route: the outcome is not deterministic, so it cannot be bisected yet

**Symptom.** With every decompiled function installed (`gt_native --decomp on`, race pad route), the run stayed in a menu (guest thread ~90 % asleep) for 200 s, where `--decomp off` reaches the race at ~45 s. The full list reproduced it three times in a row.
**Cause (not found).** Bisecting with `GT_DECOMP_ONLY` (criterion: some 5 s window under 40 % vblank sleep within 120 s) gave no single culprit. Each half of the list ran fine alone, the halves together got stuck, and a minimal-set search wandered. Re-running two sets with known outcomes then flipped one: the half that got stuck reached the race twice in a row. Whether the route reaches the race with many functions installed depends on timing. `call_guest` counts every guest step as a dispatch, which moves the preemption points and changes the PSP threads' order (see the `--decomp on` freeze entry). The pad route presses buttons at fixed vblanks, so a menu that opens a few frames later misses a press.
**Fix / next step.** Done 2026-10-19: the routes wait for the game (`WAIT:seek`, host/pad_sequence.hpp), and the real causes were three decompiled functions that dropped `$f0` / `$v1` (see "`--decomp on` showed black screens and then died ..."); the trace divergence is the scheduler's dispatch clock, not a fault. The differential gate (`--decomp diff`) was never affected.

Found: 2026-10-19 · scratch bisection scripts over host/gt_native (`--decomp on`, `GT_DECOMP_ONLY`)

## Frames skipped with the game 80 % idle on a slow CPU: GT's second vblank wait counted as "host late"

**Symptom.** On the laptop in power-saver (1.9 GHz), gt_play skipped 7-14 frames/s in menu and race windows where the guest thread slept 75-80 % of the time and barely waited for the GE (2.5 %).
**Cause.** GT waits for each vblank from two threads in the same guest instant. The first wait sleeps until the vblank's real time; the second targets the same vblank, already past, so it always arrives "late" by the work between the two waits. A probe over the race route on the laptop: ~300 of every 600 waits arrived 2-4 ms (or more) late, and every sleep woke < 0.25 ms after its due time. The frameskip took any wait more than 1 ms late as "the host cannot keep up". On a fast CPU the second wait came < 1 ms late and even cleared a first wait's real lag, so it never showed there.
**Fix.** `gt::VblankPacer` (host/vblank_pacer.{hpp,cpp}, CTest `vblank_pacer`): only the first wait of each vblank measures the lag (keyed on the target vblank, not on the thread: any repeated wait for the same vblank is skipped), and late means more than 4 ms (a quarter vblank). Measured on the desktop with two cores, race route: first waits arrive 1-2 ms late only a few times per 5 s with the game 60 % idle, which the next frame catches up without skipping, plus isolated 10-50 ms stalls. Result there: frameskip 0.10/s on average (max 1.2), guest time 1.00x. On the laptop in power-saver (A/B, two rounds each, race route, binaries run from /tmp): frameskip 3.3-3.6/s (max 18.8) before, 0.12-0.15/s (max 2.4) after; GE wait 14-15 % before, 8.3 % after. The perf log has a new `vblank lag` line: first waits, how many were late by more than 1 / 4 / 8 ms, and the maximum.

Found: 2026-10-19 · host/vblank_pacer.cpp, host/gt_kernel.cpp (laptop probe over SSH, binary run from /tmp)

## Profile-guided optimisation cuts the game thread by a third on the laptop

**Symptom.** In power-saver (1.9 GHz) the laptop's game thread ran at ~59 % of a core in the race and the GE at ~71 %. The generated code is compiled at `-O1` (memory: ~372 MB per unit at higher levels) and is full of branches.
**Cause.** Without a profile clang has to guess which code is hot, and `-O1` does little inlining or block layout. A profile from one run of the game tells it.
**Fix.** `tools/pgo.sh`: an instrumented build (`-DGT_PGO_GENERATE=ON`), a 220 s run of the race route, `llvm-profdata merge`, then `build/` built with `-DGT_PGO_PROFILE=...`; about 14 minutes. Laptop in power-saver, race route, A/B with two rounds each, binaries run from /tmp: game thread 59 % → 39 % of a core, GE 71 % → 63 %, 508 → 470 ns per draw, frameskip 0.13 → 0.06/s. The "GE wait" rose from 7 to 13 % because the game now reaches its draw sync sooner. The desktop agrees (GE ns per draw −10 to −18 %, noisy under load). Opt-in: the default build is unchanged. The profile comes from the user's own run of the game and stays out of git.

Found: 2026-10-19 · CMakeLists.txt (GT_PGO_GENERATE / GT_PGO_PROFILE), tools/pgo.sh

## The b101p pool was dry at batch 102: 62 of the 225 reached functions had neither VFPU nor an import within five calls, and the static import walk errs both ways

**Symptom.** After batch 101 the probe (`b101p`, reached by the five routes, up to 300 instructions, no `src/` definition) listed 225 functions. The hottest are VFPU matrix helpers (`func_08B53D2C` 10,089,553 calls, `func_089A9064`, `func_08B5A7F0`: `mtv`/`lv.q`/`vtfm4.q`, kept out of the decomp), the 62 without VFPU or an import stub within five calls (`jal`/`j` targets past `.text`) held the libm wrappers with 64-bit soft-float helpers (`func_08B6EF68`, `func_08B67418`), sorts, a handful of 100-300 instruction parsers and float code, so about 40 were writable.
**Cause.** The walk is an over-approximation: it marks any function that reaches an import stub, and of the 16 written functions it marked, 8 compared fine (`func_08A92588` 744,457 calls in the fast gate, `func_08B31AB4`, `func_08C0A8A0`, `func_0896264C`, `func_08BC6550`, `func_0880BBF4`, `func_08825B74`, `func_08820C0C`) and 8 were "not comparable" in every case (`func_08B35418`, `func_08AEB850`, `func_08B2DBF0`, `func_08B7771C`, `func_08B098A8`, `func_0897BB68`, `func_08825AE8`, `func_0885E940`). Most of the ones that compared reach only the stubs `0x08C7A888`, `0x08C7A898`, `0x08C7A8A8`, `0x08C7A8B0` (I did not map them to NIDs; they look like the replay-safe interrupt / lock imports of `host/main.cpp`; `func_0897BB68` reaches only those within five calls and was still not comparable, so the address is a hint, not a rule) or `0x08C7AF30` / `0x08C7AEF0` (`func_08B31AB4`: 94 cases compared of 72,135 calls); the ones that did not reach `0x08C7A948`, `0x08C7A998`, `0x08C7AF70`, `0x08C7A968` / `0x08C7A970` or whatever sits behind a virtual slot the walk cannot follow (`func_08825AE8` and its twin `func_08825B74` differ only in one callee, and only the twin compared).
**Fix / workaround.** Do not drop a function for an import in its walk: list the stubs it reaches (`.venv/bin/python -m tools.pool recomp/analysis/probe_TAG_*.log --stubs` prints the pool with the import stub addresses each function reaches) and check them against `kReplaySafeNids` / `kRecordableNids`; write the short ones and let the first fast gate decide. A sampled function with a compared case but no memory change needs a dense run before it is unregistered (`func_0888879C` and `func_08B00E90`: 3,000 equal dense cases, none changing memory, so both stay unregistered). The next batch needs the VFPU decision first (a portable `vtfm4`/`vmmul` helper) or the 300+ instruction functions.

Found: 2026-10-19 · src/game/unsorted/hot_batch_102.cpp

## A function reached by one case per run can be missing from a sabotage round: run it alone

**Symptom.** `func_08891C54` (a 2-instruction forwarder) compared one case in two of four 42-function fast runs and none in the others, so `tools.decomp_gate sabotage` reported it undetected (41 of 42).
**Cause.** The routes are real-time scripts; which one-off calls fall inside the 200 s uncapped window changes from run to run with the other installed functions' timing.
**Fix / workaround.** `sabotage s2 --only 0xADDR` with the one function installed reached it and pinned its s0 bump. Check `calls` before reading a missing detection as a hole in the sabotage.

Found: 2026-10-19 · src/game/unsorted/hot_batch_102.cpp (func_08891C54)

## Streaming GT's fast draws past the backend handshake saved only 1-3 %: the GE thread's cost is spread, not concentrated

**Symptom.** After #236 and PGO, the laptop in power-saver spends ~500 ns per GE draw. The backend handshake per fast draw looked like the next target: a mutex, a 352-byte descriptor compare, a texture-map lookup and the LOD. It is the same answer for every draw of a run.
**Cause.** Removing it (patch 0052, streamed draws: verified identical on 31.4 M draws, 85 % of fast draws streamed) moved the laptop's ns per draw from 507-508 to 493-504. A profile of the streamed build shows the remaining cost spread over many small pieces: `render_ge_primitive_pass` 5.4 %, the command loop 4.9 %, `hw_job_prepare` (non-strip draws) 4.8 %, the stream itself 6.6 %, the prescan 2.6 %.
**Fix / lesson.** Not merged (spec `2026-10-19-ge-streamed-fast-draws-design.md`, result section); the code is on branch `perf/ge-streamed-fast-draws`. Before another per-draw micro-optimisation of the GE, profile the target machine: the next gains need structure, such as more than one core decoding lists or the renderer leaving the PSP command lists behind.

Found: 2026-10-19 · laptop A/B and profile over SSH (binaries from /tmp)

## `--decomp on` showed black screens and then died: three decompiled functions dropped the register their callers read ($f0, $v1)

**Symptom.** `gt_native --decomp on` on the race route (the "does not always reach the race" and "froze" entries above): every frame black from the title movie on (mean brightness exactly 0.0 in all dumped frames, while the page loads kept coming), and once the screens drew, `terminate called ... Guest memory access outside PSP RAM at 0x2C6C6E4C` (ASCII "Lnl,") when the race loaded. The differential gate was green throughout.
**Cause.** Not timing. `tools.trace_bisect --black-after 3300` (ddmin over the 6,622 registered functions, 16 runs) isolated `func_08B71EC8`; `--crash` (21 runs) isolated `func_08A6C134`. `func_08B71EC8` is the double-to-float narrowing: the original ends every path with `mtc1 $a2, $f0`, so the float comes back in **$f0**, and the C++ set only `v0`. Its caller `func_08A6C158` (and the decompiled parser in `hot_batch_94`, `put_float(.., ctx.fpr[0])`) read `$f0`: every float parsed from a layout string was 0, so nothing was drawn. `func_08A6C134` calls `func_08A9F9CC`, a 64-bit integer parse (v0 low, v1 high), and returned only `v0` while registered `Word`. The gate compares what the `ReturnKind` says (`Word` = v0, `DoubleWord` = v0:v1, `Float` = f0): the three were registered with a kind that never looked at the register their callers read, so they passed their cases and the bug lived only where a decompiled caller met a decompiled callee.
**Fix.** The three return what the original returns (`Float`, `Float`, `DoubleWord`). With the `Float` kind and no fix, the fast gate on `func_08B71EC8` fails (`f0: original 0x3F800000, decompiled 0x00000000`); with both, 0 mismatches. `--decomp on` then draws the same frames as `off` and reaches the race (`race.pad`, `script done at vblank 12365`, exit 0).
**Why it isn't obvious.** A `Word` function whose callee returns a float or a 64-bit value passes the gate by construction. The first suspect for "a decompiled leaf makes on differ from off" is the callee's other result registers: read the callee's last instructions (`mtc1 .., $f0`, a write to `$v1`) before the C++. Other `Word`-kind functions that forward a float or 64-bit callee are not audited yet.

Found: 2026-10-19 · src/game/unsorted/hot_batch_76.cpp, src/game/unsorted/hot_leaves.cpp (func_08B71EC8, func_08A6C158, func_08A6C134), tools/trace_bisect.py

## `--decomp on` and `off` can never share an import trace: the scheduler's clock counts dispatches, and a decompiled function changes the count

**Symptom.** With the three fixes above `on` and `off` follow the same route to the same race screens, but their import traces leave each other at import 4,344 (`cmp` of two `--trace` files, first 200,000 lines; two `off` runs are identical, two `on` runs are identical). It is the same point as the 4,406 of the freeze entry: it never depended on the pad route (the first press is at vblank 3300; the divergence is in the first seconds).
**Cause.** `tools.trace_bisect` (trace criterion) finds `func_08B35ED0`, `strchrnul`, a pure leaf the gate verified, alone: with only that function installed the trace leaves `off`'s at the same place. The lcs-recomp runtime advances virtual time and preempts threads every `PSPRECOMP_TIME_TICK_DISPATCHES` (256) dispatches since the last import (`dispatches_since_import_`, `lcs_starvation_tick`), and a decompiled function replaces a stretch of guest dispatches by one host call, so the tick lands elsewhere, the thread order changes (the first difference is a `sceKernelChangeThreadPriority` that `off` makes twice in a row and `on` does not) and virtual time drifts. Consistent with that: with the tick every 100,000 dispatches and only `strchrnul` installed, the first difference moves from import 4,345 to 5,209. Setting the tick to 0 (no preemption) hangs the game in a busy-wait, so the equality could not be tested directly.
**Fix / lesson.** None, and none wanted: import-trace identity between `on` and `off` is the wrong criterion for the decomp (it would fail with any function installed). `on` is correct when it reaches the same states: the `WAIT:seek` route finishes in both (the 12 page loads in the same order) and the race frames agree (position, lap, speeds within a few mph). Use that, plus `tools.trace_bisect --black-after / --crash`, when `on` misbehaves; do not bisect on trace equality.

Found: 2026-10-19 · host/main.cpp (`--trace` writes `<trace>.names`), tools/trace_bisect.py

## GT_PAD routes that wait for the game: times inside a segment are offsets from the last wait, not a sequence

**Symptom.** A route written as `WAIT:seek=X,300:RIGHT,100:RIGHT,200:CROSS` pressed CROSS first. Written three times in a row while exploring (the old syntax is a list of absolute vblanks, and the habit stays).
**Cause.** `WAIT` starts a new segment whose presses are offsets from the vblank where the wait was satisfied, exactly as the old syntax's vblanks are offsets from 0; the presses of a segment are scheduled together, not one after the other.
**Fix / workaround.** Write increasing offsets inside a segment (`300:RIGHT,400:RIGHT,500:CROSS`) or put a `WAIT` between presses. A wait looks only once the segment's presses are released, and counts only events from the segment's start (events caused by the segment's own presses count).
**Also:** the vblank rate is ~120 per second of wall time whether `LCS_UNCAPPED` is set or not, so a route's length in seconds is its last vblank / 120; a page is loaded 110-190 vblanks after the press that opened it, the race itself ~1250; some screens leave nothing to wait on (returning to the resident main menu loads no page; the Trade Cars notice is built in memory): those keep a time, after a wait.

Found: 2026-10-19 · host/pad_sequence.cpp, config/pad_routes/*.pad

## 97 % of a race frame's GE draws are replays of static model blobs; GT's own list code writes only ~3 %

**Symptom.** ~1 M GE draws per second in a race (about 12-15k per frame), while the functions that write GE command words in code run only ~10k times per second.
**Cause.** A census over 20,265 enqueued lists (race route, `gt_native --decomp off`, a scratch `GT_GE_CENSUS` hook in the `sceGeListEnQueue` HLE, 2026-10-20):
- **Lists are complete at enqueue.** GT enqueues one list per frame, alternating `0x08D00490` / `0x08D20490`, with stall 0 every time. GT does not import `sceGeListUpdateStallAddr`. Re-walking a list at the next enqueue and at DrawSync differed 0 times in 20,264.
- **A race list:** on average 12,432 PRIMs, 73.9k vertices, 57.6k executed words and ~300 depth-0 CALLs. 99.5 % of the CALL targets are the same as in the previous frame.
- **Where the PRIMs come from:** 97.2 % from CALLed sublists ("model blobs"), 2.8 % from code (in the race, ~62 per list: the HUD).
  - Blob memory: heap `0x096-0x099` 69 %, upper VRAM `0x041b-0x041f` 25-26 % (a model cache that copies and relocates heap blobs: `0x08B54D28`), static `0x08F-0x091` 2-5 %.
- **Blobs are static.** 2,480 of 2,547 targets kept one content hash for the whole run (84.1 M of 90.7 M PRIMs), and the vertex bytes behind 1.49 M draws were unchanged. What does change:
  - the 21-word texture-binding blocks (texture registers only);
  - the glyph sublists;
  - VRAM cache slots, refilled at race start and in two later bursts.
- **A blob** is a command list followed by its own vertices (absolute VADR): ATEST, BASE, VADR, BLEND, a CALL to a texture block, then repeated `0xC8` (texture level, once per PRIM), VTYPE `0x112`, PRIM, and RET. A model blob has 70 PRIMs on average; strips average 5.8 vertices.
- **GT's graphics library context** is at `0x08D6CE00` (`+0x5C` list cursor, `+0x60` downward vertex pool). Its hot functions:
  - `0x08B53D2C` / `0x08B53978`: immediate 2D quads (UI, text);
  - `0x08B54694` / `0x08B544B0`: glyph transfers;
  - `0x08B52578`: texture binding through prebuilt sublists;
  - `0x08B59508`: lazy matrix flush, ~150 world matrices per race list;
  - `0x08B550A8`: `callList` (BASE + CALL);
  - `0x08B54D28`: the model-blob cache lookup and relocation.
- **Corrections:** `GT_GE_LOG` agreed with the census on the list checked (14,680 PRIMs both ways); an earlier remark that lists are streamed was wrong. `GT_PROBE` does not count calls made inside one recompiled unit.
**Consequence.** Decompiling the list-writing code reaches ~3 % of the draws. The draw cost is re-decoding the same static blobs every frame, so the lever is to decode a blob once and replay it (spec `2026-10-20-ge-blob-replay-design.md`).

Found: 2026-10-20 · research run over `gt_native` race route (run logs kept out of git)

## Batch 103 unlocked 42 imports (40 recordable, 2 replay-safe) by reading their HLE: 13 functions that had been parked as "not comparable" compared at once, and the static import walk was the wrong measure

**Symptom.** After batch 102 the pool of reached, easy functions was dry and the written-but-unregistered ones (about 70 commented `HOTnn` lines with "not comparable", "reaches sce* imports that block", "the import … is not recordable") stayed out. `tools.pool --stubs` over a fresh probe (`b103p`) said `sceKernelDelayThread` blocked 140 of the 277 non-VFPU candidates (106 of them had no other blocker) — but that is the static walk, which marks a function for any import within five calls, however rare the path.
**Measure.** Uncommenting all 74 candidate registrations and running one fast gate counted the *dynamic* blockers (`first not comparable`): `kernel isolation (LwMutex contention)` and `(sleep would block)` dominate (about 45 functions; those are real contention and sleeps and stay out), then `sceIoRead`, `sceDisplay` vblank waits, `sceUtility` and `scePowerSetClockFrequency` (the last one delays the caller 150 ms in the GT HLE: never). The imports with a non-blocking HLE that blocked the most functions were semaphores (`sceKernelWaitSema` / `SignalSema`: `func_08AEB4CC`, `func_08AEB908`, `func_08AEB930`, `func_08AD96A0`), `sceSasCore` (`func_08B83C84`, `func_08B84270`, `func_08A235F8` and the four reverb setters), `sceKernelMaxFreeMemSize`, `sceKernelCreateSema`, the partition-memory calls and `sceIoClose`.
**Evidence per import (host/gt_kernel.cpp, tools/src/lcs-recomp/lcs/host/lcs_sas.cpp).** Recordable (run once in the original's run, replayed in the decompiled one; the state they change lives in host tables the replay state does not hold, which is exactly why a second execution would be wrong): `sceKernelMaxFreeMemSize` (reads the partition and stack cursors), `sceKernelAllocPartitionMemory` / `GetBlockHeadAddr` / `FreePartitionMemory` (bump allocator and a uid table; the block is zeroed through `GuestMemory`), `sceKernelCreateSema` / `CreateEventFlag` (a new table entry), `sceKernelDeleteThread` (only a Created / Completed thread), `sceIoClose` (file table erase), the sub-interrupt register / enable / release calls (table only; the handler is not called), and every `sceSasCore` call the HLE implements (voice table and mixer on the host; the mix writes its output buffer through `GuestMemory`; none waits). Replay-safe: `sceKernelPrintf` (returns 0, prints nothing) and `sceWlanGetEtherAddr` (a constant MAC). The six that can switch threads are recordable only because the kernel now refuses the blocking case inside a case, like sleep and wakeup already did: `sceKernelWaitSema` / `WaitSemaCB` when the count is below the request, `SignalSema` when it would satisfy a waiter or when a ready thread outranks the caller, `sceKernelWaitEventFlag(CB)` when the pattern is not set, `SetEventFlag` when it would satisfy a waiter or a ready thread outranks the caller (`semaphore wait would block`, `semaphore signal would wake or preempt`, `event flag wait would block`, `event flag set would wake or preempt`); `ClearEventFlag` and `PollEventFlag` never wait. Outside a case (`--decomp off` / `on`) the guards are not active. `sceKernelCreateMutex` was tried and reverted: mutexes are in the replay state (owner, count per uid), and recording its creation made the already registered `func_08B6441C` fail the full gate with `kernel state the import changes differs from the original's`; the same holds for anything whose table is snapshotted. Never: `DeleteSema` / `DeleteEventFlag` (wake the waiters, may preempt), `DelayThread`, `sceIoRead` (writes its buffer with a system call and defers the thread), `sceDisplay*`, `sceGeDrawSync`, `scePowerSetClockFrequency`, `sceUtilityLoadModule`.
**Fix.** The two NID lists moved from `main()` into `host/import_lists.hpp` and are checked by `host/tests/import_lists_test.cpp` (CTest `import_lists`; seen failing with `DelayThread` added). Result: 13 parked functions re-registered with their original code (`func_08B35418`, `func_08AEB850`, `func_08B7771C`, `func_08AEB908`, `func_08AEB930`, `func_08B84270`, `func_08B83C84`, `func_08AD96A0`, `func_08B33A24`, `func_08B59A24`, `func_08A235F8`, `func_08AEB4CC`, `func_08AF0B78`), each compared in the fast gate with 0 mismatches and its sabotage pinned. `func_08B34B2C` (a recursive-lock entry written in this batch) stays out: in 101 of 101 cases the caller owns the lock and the thread sleeps. Two further lessons: the sabotage tool inserts its bump after a `void func_X(` line, so a `[[maybe_unused]]` prefix hid `func_08A235F8` from it (0 of 1 pinned until the attribute went), and a function whose only cases are the first calls of a one-off initialiser is pinned only when installed alone.

Found: 2026-10-20 · host/import_lists.hpp, host/gt_kernel.cpp, host/tests/import_lists_test.cpp, src/game/unsorted/hot_batch_103.cpp

## Keeping GT's static strips on the GPU did not make the GE thread faster: its cost is not vertex work

**Symptom.** The blob census showed 97 % of race PRIMs replaying static models, which suggested that decoding and
appending ~74k vertices a frame was the GE thread's big cost. A vertex arena removed that work: each strip is decoded
once and drawn from GPU memory afterwards (patch 0053, ~99.9 % hits, 0 mismatches under `GT_GE_ARENA_VERIFY`, both
sabotages caught). The laptop in power-saver still spent 660 ns per draw, and the arena build was 3-5 % slower in
later windows.
**Cause.** Validating a hit costs a hash lookup over ~30k strips and a compare with a signature on the heap. Those are
random cache misses (12 % of profile samples), and they cost more than decoding bytes that are already in cache.
**Fix / lesson.** Not merged: spec `2026-10-20-ge-static-strip-arena-design.md` (result section), branch
`perf/ge-static-arena`. This is the second per-draw GE optimisation that measured flat (after the streamed draws of
2026-10-19). Do not start a third without a profile of the target machine that shows the cost it removes. On the GE
thread, a cache that is checked per draw must cost less than a sequential decode. The laptop already holds 60 new
images/s on the race route in power-saver.

Found: 2026-10-20 · laptop A/B over SSH (binaries from /tmp) and gperftools profile on the desktop
## The laptop in power-saver holds 60 new images a second on every route, windowed too; decode threads make it worse

**Symptom.** The user finds the game "malillo" on the laptop (Ryzen 7 PRO 5850U in power-saver, CPU capped at
1.9 GHz). Every earlier measurement ran offscreen, so the window, the compositor or the present path could have been
the cause.
**Cause.** It is none of them, at least on the five pad routes. The main build was run over SSH from /tmp with the
user's ini (fullscreen 1920x1080, VSync, mailbox swapchain, internal scale following the window, GPU budget 10 ms),
shown on the laptop's KDE Wayland session. Results:
- **Race:** 59.9 new images/s in every 5 s window (every image held 1 vblank), 0 late vblank waits, GPU 5.8 ms a frame,
  510-630 ns per GE draw.
- **Dealer, garage, buy, options:** 59.9/s in every window but the first one. The load at start holds one image for 4+
  vblanks. Late waits were 0 in most windows and 1-4 in a few, and the GPU needed 2.8-3.9 ms a frame.
- **`PSPRECOMP_GE_DECODE_THREADS=2`** (offscreen race) is much worse. Queued draws turn the quad-strip fast path off,
  so a draw costs 2,200-2,500 ns instead of 510-630, late waits climb to 254 in 5 s (max 83 ms), and 52-59 images/s
  are shown. Keep the default of 0.
**Fix / lesson.** No pad route reproduces a slowdown on the laptop. What the user notices must be outside these
routes (replays, other races, longer sessions, loading) or something they see but `[perf]` does not measure. A
`GT_PERF_LOG=1` log from their own play, or the moment they name, is the next input.

Found: 2026-10-20 · laptop over SSH, windowed on its display, binaries and data from /tmp (all removed afterwards)

## A route that waits for the game can wait forever under `--decomp diff`, and a wall-clock end cuts routes short

**Symptom.** With `--decomp diff` the race route timed out at Mode Selection, waiting for the Race Options seek (exit
97), although the frames showed Race Options on screen. In the full gate, 5 of 11 routes stopped before their last
press.
**Cause.** First cause: the `WAIT:seek` signal was noted only outside harness cases (`!kernel_isolated()`). That seek ran
inside a compared case, and sceIoLseek is recordable, so the original's call is the live one. Second cause: 11
parallel routes run slower than real time, so a `--max-seconds` end lands earlier in the game than it does alone.
**Fix / workaround.**
- Seeks are noted inside cases too. The decompiled run replays the call without reaching the HLE, so a seek is never
  noted twice from one case.
- Full routes end `GT_PAD_EXIT_AFTER` vblanks after their script is done ("Runtime stopped: Pad script done"), with
  the wall clock only as a cap.
- The newly reached paths found one bug: `func_08888988`'s v0 on an empty pair (drift).

Found: 2026-10-20 · full gate of feat/pad-routes-2

## Batch 104: a stopped agent's commit can carry sabotage lines; parked functions come back when their blocker is gone

**Symptom.** Batch 104's agent was stopped mid-run, and its work was saved as a WIP commit. That commit held 4
`ctx.gpr[16] += 1u; … 0x08000F00u …` lines: a sabotage round's edits, captured before the tool restored the file.
**Cause.** `tools.decomp_gate sabotage` edits the sources in place while it runs, so a commit taken mid-run keeps them.
**Fix / lesson.**
- Before committing someone else's interrupted work, `git grep 0x08000F00u -- src` must return nothing.
- The 18 re-registered functions were parked as "compared 0", or because every case reached a `sceSasCore` import the
  harness could not record. Batch 103 lifted those blockers. All 18 now compare with 0 mismatches and each has its
  sabotage pinned.
- Ten of the 33 functions compared without a case that changed memory. They write through one store followed by an
  import call, and the value they store is already there.
- `func_0887B2EC` is the weakest: 2 cases.

Found: 2026-10-20 · decomp/hot-batch-104 (taken over from the stopped agent)

## Decompiled callers dropped the VFPU state their callees left, and the harness never compared the VFPU

**Symptom.** Comparing the VFPU registers in the harness (`EndState::vfpu_bits`/`vfpu_ctrl`) turned the full gate from
0 mismatches into 258. The mismatching functions included ones with no VFPU instruction of their own: `func_08888988`
only makes virtual calls.
**Cause.**
- `call_guest` ran a callee on a copy of the caller's context and handed back only v0, f0 and v1. Whatever the callee
  left in the VFPU was lost. GT's graphics library leaves vectors and matrices there for later calls, and no calling
  convention saves the VFPU.
- Separately, two hand-written leaves (`func_0896418C` and `func_089641C4`, `lv.q`/`sv.q` copies) did not leave the
  vector in C000.
**Fix / workaround.**
- `call_guest` copies the callee's VFPU state back to the caller (`host/tests/guest_call_test.cpp`; red, then green).
- The two leaves load C000 through `gt::vfpu::load_column` (`include/gt/vfpu.hpp`).
- The full gate is back to 0 mismatches. The 258 mismatches the comparison found before the fix show that it detects
  VFPU differences end to end.
- Decompiled code that touches the VFPU must leave the same register file while it runs next to recompiled code.

Found: 2026-10-20 · feat/harness-vfpu (before batch 105, which targets the VFPU-heavy graphics library)

## Full-gate routes stalled on a WAIT about one run in two, on main too: pad presses lost under load

**Symptom.** With 11 routes in parallel, one route (usually options, once challenge2) sometimes waited 12,000 vblanks
for a page load that never came (exit 97). The same route alone, or in 11 parallel copies of itself, always passed.
main had it too, so it was not the VFPU change.
**Cause (most likely).** Under the gate's load the game skips frames and counts several vblanks per frame, so a
6-vblank scripted press can fall between two pad reads. Every stall happened right after a page where the route
presses buttons on fixed timings.
**Fix / workaround.**
- `PressLatch` (`host/pad_sequence.hpp`, tested) holds a scripted press until the game has read the pad twice.
- Since the fix, 4 of 5 full gates were clean. The one stall that remained (options, latch1) is unexplained, but any
  WAIT timeout now prints the PSP thread table (`[threads]`), so a deadlock would show.
- A gdb SIGSEGV inside a harness case is not a crash: the dirty-page tracker write-protects pages and handles the
  fault. Use `coredumpctl` for real crashes. One run hit a real SIGSEGV in the software rasterizer
  (`rasterize_rectangle`, worker thread), which has not been reproduced since.

Found: 2026-10-21 · feat/harness-vfpu

## VFPU code decompiles bit-exactly when each VFPU operation is the recompiler's own semantics

**Symptom.** Every batch until 104 skipped VFPU functions. That left out GT's graphics library core (matrices,
transforms, sin/cos, visibility tests), the hottest code the routes reach.
**Cause.** A decompiled VFPU function must leave the same VFPU registers, control registers included, as its original,
and the harness now compares them (#258). The recompiler's VFPU semantics have quirks a plain rewrite would miss: source
and destination prefixes, `vtfm` rewriting the destination prefix, `vf2i*` rounding through double, and `vfim`'s
half-float decode.
**Fix / workaround.** `include/gt/vfpu.hpp` gives one helper per VFPU operation (`transform`, `matrix_multiply`,
`binary`, `unary`, `float_to_int`, `pack_ints`, `rotation`, `compare3`, `prefix`, …). Each is lcs-recomp's codegen for
that operation (MIT), written once as a function over the context's own accessors. Batch 105 wrote 29 VFPU functions on
these helpers, and all were bit-exact on the first fast gate, the polynomial sin/cos included. Register numbers are the
instruction's 7-bit fields; decode them from the word as `tools/src/lcs-recomp/tools/codegen_main.cpp` does, and note the
VFPU mnemonic next to each call. Stage 2 of the no-emulation plan retypes this state; until then a function's VFPU body
stays one helper per instruction.
- A function list entry can run past a tail jump: `func_08B56790` and `func_08B5606C` are 8 instructions, and the rest
  of their listed size is the next functions.
- `tools.pool` and the probe symbol list missed registrations by address alone (`GT_DECOMP_EMPTY(0x…u)`) and offered 8
  done functions again. `done_addresses` now reads them (test).

Found: 2026-10-21 · decomp/hot-batch-105

## The software rasterizer's row pool counted one worker twice, and a worker then crashed on a dead stack frame

**Symptom.** Now and then a gate route exited 139: about one full gate in five, later a fast gate's drift route. The
core dumps all stopped in `rasterize_rectangle`, on a `RowWorkerPool` thread. The same route alone did not crash.
Earlier a gdb run seemed to reproduce it, but those SIGSEGVs were the harness's dirty-page tracker (handled faults).
**Cause.** `RowWorkerPool::run` waited only for the participating workers. A worker that took no part and woke late
could still read generation N while the next `run()` had already written `active_participants_` for N+1, which happens
before N+1 is published. That worker then ran N+1's chunks as if they were N's, ran them again once N+1 was published,
and decremented `pending_` twice. `run()` returned while another worker was still inside the body, a lambda whose
captures live on the caller's stack. The rows it wrote went through dangling references.
**Fix / workaround.** lcs-recomp patch 0052: every worker acknowledges every generation, whether it took part or not.
`run()` returns only when all of them have seen it, so none is still inside the old generation when the next one's
fields are written. The fast gate that had crashed passed twice after the fix. The race needs a late wake, so it is
rare by nature.

Found: 2026-10-21 · core dumps via `coredumpctl debug`, fix/row-pool-race

## Batch 106: the 11 routes reached trivial accessors nobody had registered, and menu presses spaced 100 vblanks apart could be lost

**Symptom.** A probe over all 16,024 pending functions on the 11 full routes turned up about 40 one- and two-instruction
accessors, all reached for the first time by the new routes. Separately, `challenge2` and `options` still stalled in an
occasional full gate. The thread table showed a live game that had loaded a different page.
**Cause.**
- The accessors were simply new coverage.
- The stalls came from menus that take presses every 100 vblanks. Under the gate's load the game runs fewer frames per
  vblank, so the cursor's animation swallowed a press and the next CROSS opened the wrong page.
**Fix / workaround.**
- Accessors are registered by shape, each matched instruction for instruction from `tools.disasm`, reusing
  `src/core/leaves.cpp` and `accessors.cpp`. New shapes: `field_plus`, `deref_float`, `set_byte`.
- 15 of them only the full routes reach. The sabotage runs on the fast gate, so they stay parked until a fast route
  reaches them.
- The main-menu presses of the challenge routes and the options-page presses are now 200 vblanks apart.
- Before adding a template shape, grep `src/core` for its body: `float_getter`, `set_field` and `byte_getter` already
  existed in `leaves.cpp`, and the duplicates failed to link.

Found: 2026-10-21 · decomp/hot-batch-106

## `decomp_gate coverage` with a full gate's tag overwrites that gate's route logs

**Symptom.** Right after a clean full gate (batch 109, 5618 compared), adding up its `[decomp]` lines per function gave
80 of 83 functions "never compared", and 7 of the 11 route logs had no `[decomp]` lines at all.
**Cause.** `coverage TAG` runs a fast gate that writes `recomp/analysis/gt_diff_TAG_<route>.log`: the same files the full
gate wrote. Given the full gate's tag, it rewrote the logs route by route while they were being read.
**Fix / workaround.** Give `coverage` its own tag (`b109cov`, never the full gate's). The full gate had to run again.
Its per-function counts live only in those logs.

Found: 2026-10-21 · decomp/hot-batch-109

## A `Word` return compares v0 even where the function returns nothing: early exits leave the value their test read

**Symptom.** Batches 110 and 111 had 8 mismatches, all `v0: original X, decompiled Y`, in functions whose body was
correct (memory and the other registers matched).
**Cause.** `ReturnKind::Word` compares v0 on every case. A `void` function's early exit in the asm leaves v0 = the value
its last test loaded (`lbu v0, 0x25(v1); bnez v0, exit`), or an address the code computed (`addu v0, a1, s2` before a
load). The C++ returned with v0 = the previous callee's result.
**Fix / workaround.** On each early exit, set `ctx.gpr[2]` to what the asm's last v0 write left: walk the exit's path back
to the last instruction that writes v0. A table of tests becomes a loop that stores each value read in `ctx.gpr[2]`
before testing it.

Found: 2026-10-21 · decomp/hot-batch-110, decomp/hot-batch-111

## Eleven full-gate routes outgrew the slice: the options route stalled at `MemoryHigh`

**Symptom.** The options route of three full gates in a row (batches 110 and 111) ended `WAIT seek=722883648 timed
out` with exit 97, also on an idle machine, while every other route passed with 0 mismatches. Alone, in real time or
uncapped, with all functions, only the batch's or `--decomp off`, it passed every time.
**Cause.** A route's peak RSS grew with the build to ~0.5 GB (506 MB measured); the full gate started all 11 at once,
~5.6 GB, past the slice's `MemoryHigh` (5 GB; `MemoryPeak` read 5.37 GB). The kernel throttles the slice there, and a
real-time route that falls behind stops seeing the seek its pad script waits for.
**Fix / workaround.** `route_shell` keeps at most `MAX_PARALLEL_ROUTES` (8) full-gate routes alive, longest first
(bash `wait -n`), so the race still sets the gate's length. When a real-time route stalls only inside the gate, read
`systemctl --user show gt-heavy.slice -p MemoryPeak` before bisecting functions.

Found: 2026-10-21 · decomp/hot-batch-111

## `tools.disasm` pairs a `lui` with the next low half in address order, not on the path that reaches it

**Symptom.** In `func_08AEE144` the log call at 0x08AEE2E4 printed `lw a0, 0x3640(v0) [LO] = 0x08D3FB40`, while the
other log sites of the function load 0x08CDFB40.
**Cause.** 0x08AEE2E4 is reached only by the branch at 0x08AEE210, whose delay slot sets `lui v0` (the HI half that
gives 0x08CDFB40). The tool annotates each LO with the latest `lui` of the register in address order, here the
`lui v0, 6` at 0x08AEE218 that the branch skips.
**Fix / workaround.** At a LO that is a branch target (or right after a branch whose delay slot loads the same
register), take the HI from the `lui` on the path that reaches it; the relocation table pairs them correctly. The
tool itself is not fixed yet.

Found: 2026-10-21 · src/game/unsorted/hot_batch_115.cpp (func_08AEE144), independent review of the draft

## Another session's niced jobs still stall real-time routes: give the gate slice a high CPU weight

**Symptom.** With the shared CPU lock held (no other timing job running), full gates still lost a route to `WAIT seek
… timed out` while another agent session ran its builds and tests at `nice 19` (load 27-36 on 24 cores).
**Cause.** `nice` only orders threads inside one cgroup; between cgroups the kernel shares CPU by `CPUWeight`, and the
gate's slice had the default (100), the same as the other session's.
**Fix / workaround.** `tools.decomp_gate` sets `CPUWeight=10000` on `gt-heavy.slice` with its memory limits (runtime,
no root). The next final gate passed every route at load 27-36.
**Not the whole story** (batches 121-122). The weight did not stop it under a mutation-testing run (4 workers, ~11 of 24
cores, `nice 19`): two finals in a row lost a route (`buy` seek wait, then `manual` / `circuit` past `--max-seconds`).
With that run frozen while the lock is held (the other session now `systemctl --user freeze`s its cgroup whenever
`cpu-heavy.owner` names a live holder), one more final still lost `challenge`: 9 seeks after the last satisfied wait, none
to the awaited offset, so the game took another path, not merely a slower one. The rerun passed. Every loss is on a
different route, so it looks like route flakiness that load makes likelier; untested yet whether `--decomp off` loses
routes too. Rerun the final gate; a route problem is never counted as a pass.
**Then measured** (batch 123, other session frozen): each route keeps 2.6-3.1 cores busy in `top` (game, GE worker,
audio threads), so the gate's own 8 routes filled the 24 cores; `options` lost three finals in a row, twice at the same
menu step (the game alive, waiting where a press should have taken it). `MAX_PARALLEL_ROUTES` is now 6 (18 cores, a test
keeps it under 80 % of the machine); the full gate grows from ~600 s to ~705 s.

Found: 2026-10-22 · decomp/hot-batch-119; 2026-10-22 · decomp/hot-batch-122

## Uneven frame pacing comes from the game thread missing vblanks while it waits for the GE thread, not from presenting

**Symptom.** On the laptop the user saw irregular pacing and drops below 60 fps, while every `GT_PERF_LOG` average said
"presented 59.9/s": 5-second averages cannot show an uneven interval.
**Measured** (`GT_FRAME_LOG`, desktop, real Wayland window, Vulkan mailbox, race route 60–220 s, `python -m
tools.frame_pacing`): even on the fast desktop, 4.4 % of new-frame intervals exceed 17.5 ms (p99 33.3 ms, max 66 ms;
125 intervals of 30–36 ms, plus short 5–12 ms intervals right after late ones). `present_frame` itself is short (p99
1.5 ms). Of 152 intervals longer than 25 ms, 136 are long in guest time too: the game thread flipped a vblank late. The
vblank waits show why: 84 arrived more than 2 ms late (max 43 ms), and the game thread spends 7–13 % of its time in
`sceGeDrawSync` waiting for the GE worker.
**Cause.** A frame whose CPU work plus its wait for the GE thread exceeds one vblank flips a vblank later (a 33 ms
interval), and the catch-up after it gives short ones. On the laptop in power-saver the GE thread runs near saturation
(~660 ns per draw, ~1.2 M draws/s in a race), so those waits and late vblanks are far more frequent there.
**Fix / workaround.** Measure the laptop with `GT_FRAME_LOG` before choosing a fix; the levers are the GE thread's
throughput and how long the game thread blocks in `sceGeDrawSync`. `VK_KHR_present_wait` (patch 0053) adds the
screen-side times (`D` rows) where the driver offers it; it reported off on the desktop's NVIDIA/Wayland surface.

Found: 2026-10-22 · branch perf/frame-pacing-measure (GT_FRAME_LOG, tools/frame_pacing.py)

## A callee reads argument registers the asm set long before the call

**Symptom.** Batch 123: `func_0894DD9C` wrote a gear request one higher than the original (constants+0x732) on two
routes; `func_089193B8` intermittently faulted at guest address 0x23 in the decompiled run only.
**Cause.** Both called a function with fewer arguments than it reads. `func_0895DD80` compares the speed against f12,
which the original sets (`mov.s $f12, $f0`, the controller's limit) a few instructions before the `jal`;
`func_08918C84` starts with `lw $a2, 0x20($a1)`, and the original's a1 is the timer it loaded for an earlier test,
not set next to the call at all. `call_set` / `call_setf` leave unlisted registers as the previous callee left them,
so the decompiled call saw garbage there.
**Fix / workaround.** Before writing a call, read the callee's first instructions for every a0-a3 / t0-t3 / f12-f15
it uses, and pass each one with the value the asm holds at the `jal`, wherever that value was loaded. The shape that
hides it: the caller's own cases pass while the callee's garbage happens to match (here 2 of 18 cases, and a fault
seen in one gate of three).

Found: 2026-10-22 · decomp/hot-batch-123

## A loop the compiler entered with `j` into its middle calls the producer once more than the C reads: the first token is dropped

**Symptom.** Batch 124's first full gate: `func_08A0B264` (a style colour parser) left `RAM at 0x09FB15E2: original 80, decompiled 00 (3 bytes differ)` — one colour channel off.
**Cause.** After `jal func_08987A74` (next token) the asm copies the defaults and `j`umps to the loop's *own* `jal func_08987A74` before the first test, so the first token is read and dropped and the loop starts on the second; written as "kind = next(); while (kind != -1) …" the C used the dropped token as the first value. In the same batch `func_08811684` faulted at 0x4 because `beq $s0, $v0` right after `jal func_089DDC94` was read as a test of the slot count against a constant: it compares the callee's answer with the wanted count, and the layout setup runs only when they differ.
**Fix / workaround.** Follow the `j` target of a loop head before writing the loop, and read the operand of a `beq`/`bne` after a `jal` as the callee's `v0` unless an instruction in between redefines it.

Found: 2026-10-22 · src/game/unsorted/hot_batch_124.cpp (func_08A0B264, func_08811684)

## GT drew ~80 frames per second for a 60 Hz screen: the vblank handler ran on every vblank wait

**Symptom.** On the laptop, Time Trial on the start straight lost ~5 images per second (55 new images/s, 684 repeats in 140 s, 611 intervals of 30–36 ms), the scrolling text under the menus moved in jerks, the intro video ran at ~16 fps (PPSSPP: 30), and the GE thread had ~25 % more work than needed. The GE was not the cause: in the frames that missed a refresh the game waited 0.7 ms for it and its lists took 1.8 ms (`GT_FRAME_LOG` S rows, `tools.frame_pacing` "missed refreshes").
**Cause.** GT's render thread (uid 6) paces itself by a frame counter (0x08D6CE20) that only its vblank sub-interrupt handler (0x08B5A330) advances. Two threads wait for each vblank, and `wait_vblank` ran the handler on every wait: ~100 times per second. The render thread flipped ~80 times per second (one or two flips per vblank period), so the 60 presents per second sampled them unevenly. PPSSPP's trace has 10,469 flips for 21,169 vblank waits: one flip per vblank.
**Fix / workaround.** The handler runs once per vblank, on the first wait that wakes at a boundary not yet dispatched (`dispatch_vblank_interrupt_if_due`). A first attempt also let a long `sceKernelDelayThread` book the handler for its own wake-up; every vblank before it then went without one and the game froze — a wait now takes the handler only when it ends at the next boundary. Capping flips at one per vblank in `sceDisplaySetFrameBuf` instead (holding the render thread) was tried and dropped: it treated the symptom and made the menus' loading ~1.6x slower. Result on the laptop in power-saver, Time Trial: 59.9 new images/s, 4 repeats, 0.9 % of intervals over 17.5 ms, p99 17.5 ms; the intro video decodes at ~30 fps.

**Also gone, probably:** a crash at the start of a Time Trial (guest memory access at 0x0A000000 in func_089467AC, a curve table at car state+0x448 not built yet) hit 2 of 5 laptop runs before the fix and 0 of 6 after it. With the frame counter ~1.7x fast, frame-counted waits ended before the car's set-up did. Not proven; recorded so a recurrence is recognised.

Found: 2026-10-22 · host/gt_kernel.cpp (wait_vblank, dispatch_vblank_interrupt_if_due)

## `decomp_gate sabotage` restores the batch file from a snapshot: edits made while it runs are lost

**Symptom.** During batch 134 a new function (func_08BD7974) and its helpers, added to the batch file while a sabotage run of the same file was in progress, were gone when the run ended; the build then had no trace of them.
**Cause.** `sabotage --file F` copies F before flipping s0 at each function's entry and writes that copy back at the end. Anything written to F in between is overwritten with the snapshot.
**Fix / workaround.** Never edit a batch file while a sabotage of it runs; work on the next function in another file or wait for the run to end.

Found: 2026-10-23 · tools/decomp_gate.py (sabotage)
## Trial Mountain's hills were see-through: the GE's doubled blend factors are 2A and 1 - min(2A, 1)

**Symptom.** In a race on Trial Mountain the rock walls and hills behind the track looked washed out and see-through, white where they cross the sky; the same in the GPU renderer and in the software rasterizer, so the GPU gate (GPU against software) could not see it.
**Cause.** GT draws those layers (CLUT4 textures whose CLUT alpha is 0x80, PS2-style "opaque") with blend mode 0x76: source x DOUBLE_SRC_ALPHA + destination x DOUBLE_INV_SRC_ALPHA. lcs-recomp's rasterizer (`ge_renderer.cpp` `blend_factor`) and our shader (`ge.frag`, patch 0021) took the inverse as min(1, 2(1 - A)), which for A = 0x80 keeps 99.6 % of the destination: the layer was added on top of the sky. The GE (PPSSPP's software GPU, `GPU/Software/DrawPixel.cpp`) computes 1 - min(2A, 1) = 0 here, and 2A without clamping it to 1 (the sum is clamped). Same for the destination-alpha pair (8, 9).
**Fix / workaround.** Patch 0056 in both renderers; `host/tests/ge_blend_test.cpp` pins the four factors. Found with `GT_GE_LOG` + `GT_GE_LOG_DETAIL` on one race frame: the layer's blend mode and CLUT alpha were the clue; the textures themselves decoded correctly.
**Why it isn't obvious.** Both renderers shared the wrong formula, so every GPU-against-software comparison passed. A visual bug both renderers agree on needs PPSSPP's source (or PPSSPP itself) as the reference.

Found: 2026-10-23 · tools/patches/lcs-recomp/0056-ge-doubled-blend-factors.patch

## Exit 97 at the main menu: the routes pressed before the Welcome screen was over

**Symptom.** Full gates failed now and then on one route (manual, challenge2, options — a different one each time) with exit 97: `WAIT seek=… timed out after 12000 vblanks (N seeks since vblank ~5270, none to …)`, always the wait right after the main menu.
**Cause.** The main menu's page load (`0x1FA48900`) comes ~800 vblanks before the menu shows (the Welcome screen runs first) and every route pressed 1700 after it. With six routes in parallel and another session's jobs, the Welcome screen took longer than that, the first press went to it, and the route never reached its next page. The wait is in vblanks, which keep wall-clock pace while the starved game thread does less per vblank.
**Fix / workaround.** 2300 instead of 1700 in all twelve routes (every press of that line moved by 600). A route that fails this way is a gate failure (`route_problem`), never a pass; rerun, and if the same wait times out again, look at the frames before raising the margin again.

Found: 2026-10-23 · config/pad_routes/*.pad

## Twelve fast-gate routes at once: every route stalled to its timeout (exit 124, no report)

**Symptom.** With the twelfth route (trialmtn) every fast gate failed: all routes `exit 124` after 1000 s, logs ending at `Running from entry`, the same with one verified function (`--only 0x088DDB6C`) as with fifteen. Each route run by hand in a bare shell passed.
**Cause.** The fast gate (and the probe) started every route at once in one scope of `gt-heavy.slice` (MemoryHigh 5 GB). Eleven routes at ~0.5 GB fit just below it; twelve passed it, the kernel throttled the whole scope and nothing progressed. The full gate already ran at most `MAX_PARALLEL_ROUTES` at a time for the same reason (entry above: "Eleven full-gate routes outgrew the slice").
**Fix / workaround.** `route_shell` and `probe_shell` cap both gates and the probe at `MAX_PARALLEL_ROUTES`, longest first (tests run the generated line with a stand-in binary). Adding a route means checking it against this cap, not against the slice.

Found: 2026-10-23 · tools/decomp_gate.py (route_shell, probe_shell)

## A sabotage run stopped by `timeout` left its sabotage in the source

**Symptom.** After a run of the sabotage loop came back with no result line, `git status` showed `src/game/adhoc/mrandom.cpp` modified: `GetValue` still began with the sabotage (`ctx.gpr[16] += 1u; …0x08000F00…`). The next file's run was built with it.
**Cause.** Each run is capped by `timeout 1800`, which sends SIGTERM. Python's default for SIGTERM ends the process at once, without running `finally`, so `_sabotage`'s restore of the snapshot never happened.
**Fix / workaround.** `tools.decomp_gate` turns SIGTERM into `SystemExit` (`exit_on_sigterm`) so the restore runs; the test kills a child with SIGTERM mid-`try`. After any interrupted sabotage, check `git status src` before trusting the next run, and redo the runs built after it.

Found: 2026-10-24 · tools/decomp_gate.py (exit_on_sigterm)

## The first save route found a wrong branch in a function the gate had passed for days

**Symptom.** With the save routes, `func_0895C394` (the drivetrain step, batch 123) mismatched on `save_trialmtn` only: the
engine speed `settings + 0x590` differed by a few ulps, then everything after it. Isolated (`--only`), the same.
**Cause.** After `bnel $v1, 7, …` with `lbu $v1, 0x12($s7)` in its delay slot (a branch-likely: the delay slot runs only
when the branch is taken), the next test `== 4` reads the drivetrain's byte `+0x12`, not the layout `+0`. The C tested the
layout. The empty-save routes only drive layouts 0 and 1, so neither the C nor the original ever took that branch there;
the save's car has layout 4, and the C added `func_089479EC`'s torque the original does not.
**Fix / workaround.** Read `+0x12` there. Two lessons: a branch-likely's delay slot that reloads the compared register
changes what the next test reads (read the asm, not the shape); and a function "verified" by the gate is verified on the
paths its routes drive — a route with other cars, tracks or modes is how such a branch shows up (the lessons file:
"the gate only proves the paths it runs").

Found: 2026-10-24 · src/game/unsorted/hot_batch_123.cpp (func_0895C394)

## Every gate route stalled (exit 124, empty logs) with nothing else running: tmpfs files charged to the slice

**Symptom.** Three fast gates in a row: every route exit 124, logs empty or stopped at `[decomp] mode=diff`; the orphaned
routes sat in state R at ~50 % CPU, one thread, 70-300 MB, far past `--max-seconds`. One route alone ran fine; six in
parallel stalled, with any function set installed (even one that had passed that morning). The other session's load was
a red herring (it was pinned to six cores and the stall stayed).
**Cause.** `gt-heavy.slice` had 4.1 GB charged while idle: `memory.stat` said `shmem 4106944512`. Frame dumps
(`GT_FRAME_DUMP`) and save copies written to `/tmp` (tmpfs) by runs inside the slice stay charged to it as shmem, and with
`MemorySwapMax=0` they cannot be reclaimed. Six routes on top crossed `MemoryHigh`, and a throttled route makes no
progress.
**Fix / workaround.** Delete exploration frames when done, and check
`grep -E '^(shmem|file) ' /sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/gt.slice/gt-heavy.slice/memory.stat`
before blaming code or load: idle, shmem should be near zero. Write large dumps to a disk directory (`recomp/analysis/`),
not tmpfs.

Found: 2026-10-25 · gt-heavy.slice, GT_FRAME_DUMP

## Synthetic cases: the first sweep found a real bug and three kinds of drift on paths no route takes

**Symptom.** The first `GT_SYNTH` sweep over all 7368 decompiled functions (4 generated cases each, 70 s): 4001 with
comparable cases, 20 with mismatches, although every one of them passes the route gate.
**Causes, by kind.**
- A real bug: the race-parameter flag getters used `> 0` on the signed byte; the original is `sltu v0, zero, v0`
  (non-zero). Routes only store 0 / 1, so -1 never showed. Two callers (`func_088DE9CC`, `func_088D6310`) mismatched
  only through it.
- Leftover registers a `Word` function returns: v0 = the empty string rep on `release_string`'s early return (hot
  batches 110 and 112), v0 = a `lui` on `func_088B1550`'s early return, v0 = 1 on `object.defined`'s.
- Eager reads: `tree_find` / `tree_lower_bound` read the key and `object.defined` read `*self` before the checks the
  original makes first (an empty tree, argc); with a bad pointer only the C++ faulted.
- The harness itself: trivial "return" functions in the arena that leave v0 alone made a caller's v0 the jump target
  in the original and 0 in the C++ (`call_guest` does not load the target into v0) — the arena now only uses
  `jr ra; li v0, K`. The arena also must not depend on which functions are installed (GT_DECOMP_ONLY), or a
  single-function rerun sees other inputs.
**Fix / workaround.** All of the above fixed, and later sweeps found more of the same: the six CarDriverParameter flag
getters (`> 0` again), `over_entry_max` (the raw `online_on` byte instead of its getter's non-zero test),
`func_08B212B4` (a signed `< 2` where the original has `sltiu`), and leftover / eager-read cases in `func_0882AA38`
and `func_08957B30`. Still open (8 cases, 2026-10-25): `func_088FB970` (s0 at exit), `func_089687D8`, `func_08968850`,
`func_0896B4D4` (bytes written), `func_08AD1990` (one VFPU bit), `func_08AFD484`, `func_08C18650` (only the C++ faults)
— not explained yet; one may be the time bound (the decompiled loops have none). Also: a case can loop inside one
recompiled function where the dispatcher's step / time bounds never run, and a stop inside a case stopped the whole
sweep — hence one forked child per function, killed after 10 s.

Found: 2026-10-25 · host/diff_harness.cpp (synthetic cases), src/game/race/race_parameter.cpp

## Pure byte setters escape the automatic sabotage

**Symptom.** `tools.decomp_gate sabotage` left `set_boost_param<0..2>` (race_parameter.cpp) undetected, alone or not,
although each was compared 20 times.
**Cause.** The tool bumps s0 and a RAM word; these setters use no s0, and their one store overwrites the bumped word.
**Fix / workaround.** Sabotage the store by hand (`value + 1`): 20 of 20 cases mismatched for each, file restored.

Found: 2026-10-25 · src/game/race/race_parameter.cpp

## Synthetic inputs must not depend on what is registered

**Symptom.** After batches 138-139 the synthetic sweep showed two "new" mismatches in functions decompiled long before
(`physics_pro`, `over_entry_max`) and lost one old one.
**Cause.** The arena's trivial "return" functions left out every registered function; each batch registered some of
them, so every function's generated inputs changed with each batch.
**Fix / workaround.** The trivial functions are chosen from the code alone (a decompiled one runs its decompiled
version in the decompiled run, which its own cases already compare). The two "new" ones were real bugs anyway.

Found: 2026-10-25 · host/diff_harness.cpp (run_synthetic_cases)

## A family's rows with the wrong wrapper: `--check` flags the old rows, not the new ones

**Symptom.** Rows generated with `tools.family_members TEMPLATE --cpp WRAPPER` for `pair_call_b`,
`guarded_static_object`, `target_setting_row` and `ptmf_chain_fill` made `--check` report BAD on rows that were
already in the file (37 in hot_batch_70), and ptmf_chain_fill's came out without arguments.
**Cause.** The family whose template I took is not the family that wrapper's rows belong to (another shape uses the same
wrapper, or the wrapper takes other columns); the new rows outnumbered the old ones and moved the column map.
**Fix / workaround.** Run `--check` on the file before and after adding rows; any new BAD (old rows included) means
the family / wrapper pairing is wrong — revert those rows. register_names and handle_wrapper paired cleanly.

Found: 2026-10-25 · tools.family_members, src/game/unsorted/hot_batch_70/73/74/79.cpp (rows reverted)

## Family rows that would have been wrong: three guards in tools.family_rows

**Symptom.** Generating batch 141, `tools.family_rows` produced rows that looked fine and compiled: 81 rows that all
registered the template's own C++ function at the members' addresses; RACE_OPERATOR / ORGANIZER rows copying the
template's named function; and argument-less wrapper rows (`ptmf_tail_arg(rt, ctx)`) for members that differ from the
template in a constant the row cannot carry.
**Cause.** A registration that names a function (`GT_DECOMP(0x...u, gt::game::func_X, ...)`, `HOT104(.., func_X, ..)`,
`RACE_OPERATOR(.., set_with_ghost, ..)`) is one C++ body with its own constants, not a family macro; and a row passes
only the template's arguments, so any other value position where a member differs runs the template's value.
**Fix / workaround.** The tool now (1) tries the template's wrapper row before its registration, (2) refuses a macro
row with any argument that is not a literal or true/false, (3) refuses a member that differs from the template at a value
position no argument carries, and (4) refuses a member whose words differ with no value explaining them
(`family_members.uncovered`). Batch 139's merged rows were re-checked against (3): all pass. Also: filter generated
rows against `src/` (definitions and commented registrations) before inserting them — a cleanup script that edited
"from the batch marker on" deleted existing rows further down the file (caught by the build, reverted).

Found: 2026-10-25 · tools/family_rows.py

## Live objects as synthetic inputs: +182 compared functions, an empty virtual's v0, and new open mismatches

**Symptom.** With pointer inputs drawn from the game's own objects (spec addendum 2026-10-26), the full synthetic sweep
compared 5993 functions instead of 5811, and 15 mismatches appeared in route-verified code.
**Cause.** Real objects reach paths the invented arena never did. Four were one pattern: a virtual call
(`lw v0, slot(vt); jalr v0`) into an empty virtual (`jr ra; nop`, e.g. 0x08C1F880) leaves v0 = its own address,
which a Word function then returns; `call_guest` does not load the target into v0.
**Fix / workaround.** `gt::game::hot::call_indirect` (include/gt/game/hot_common.hpp) loads v0 = target before the call;
used in func_088A3718, func_088FBB70, func_08A0F514 and func_08AD5ED8. Still open (edge paths, route-verified code):
func_08896780, func_0889E01C, func_089170E0, func_0893D9AC, func_08972594, func_08972938, func_089D7DB8,
func_08A0A398, MProductInformation.GetParentalLevel (0x08A27F20), func_08BD2D00. Later (batch 147) the 181 inline
virtual calls `call_guest(rt, ctx, word(rt, word(rt, ...) + SLOT), ...)` in functions whose asm has only `jalr v0`
became `call_indirect` (103 sites in functions with another jalr register left alone), which also fixed
func_08965CE8 and func_08A52824. The set of live objects
depends on the route's timing; two sweeps on the same build found the same 5993.

Found: 2026-10-26 · host/diff_harness.cpp, host/synth_case.cpp

## synth_batch read a run that never started as "everything compared"

**Symptom.** Re-enabling old commented rows, `tools.synth_batch` reported "275 of 275 compared", then every sabotage
"0/N detected", and commented the whole batch as undetected.
**Cause.** Two of the addresses were commented in other files too (old batches); re-enabled together they were
registered twice, so gt_native refused to start: an empty report has no "compared 0" lines, which the tool took as
"none never compared", and the sabotaged builds failed the same way.
**Fix / workaround.** The tool stops without editing when the run did not finish ("Synthetic cases done" and a
per-function report) or a sabotage printed no summary, and it never counts a function with a mismatching case as
verified. Re-enable only addresses that appear once in `src/`.

Found: 2026-10-26 · tools/synth_batch.py

## Six routes on the cores another session leaves free: full gates lost seeks

**Symptom.** On 2026-10-26 four full gates in a row each lost one to five real-time routes to `exit 97`. Each time a
pad `WAIT seek=…` timed out after 12000 vblanks: `save_drift`, `save_trialmtn`, `options`, `challenge2`. Run on their own
right after, the same routes finished with 0 mismatches.

**Cause.** The engine-sim session's mutation and profiling jobs are pinned to CPUs 0-5, so 18 cores were left.
`MAX_PARALLEL_ROUTES` was a fixed 6, and six routes at ~3 cores each filled all 18. The check
"routes × 3 ≤ 0.8 × cores" counted `os.cpu_count()` (24), not the cores the gate could actually use.

**Fix / workaround.** `tools.decomp_gate` now counts the process's CPU affinity: `MACHINE_CORES` is
`len(os.sched_getaffinity(0))`, and `max_parallel_routes(cores)` keeps a fifth of those cores free, up to 6 routes. While
another session holds CPUs 0-5, run gates as `taskset -c 6-23 .venv/bin/python -m tools.decomp_gate …`. That gives 4
routes and about 22 min per full gate.

Found: 2026-10-26 · tools/decomp_gate.py
