# FACTS — gran-turismo-decomp

This is the repo's **shared working memory between agents**: facts **verified against the binary, the
repo or the running system** that a fresh agent would otherwise spend time rediscovering. Read it when
you start, append to it when you finish (see
[Agent orchestration](agents/agent-orchestration.md#agent-orchestration--parallel-where-its-free-batched-where-its-yours)).

Not here: gotchas that are *not* deducible from the binary or the code — those go in
[`FINDINGS.md`](FINDINGS.md). Decisions go in `AGENTS.md` or a spec. A line without `verified:` is a
rumour; one fact per line, newest first inside its group. Addresses are **ELF vaddr (base 0)** unless
the line says otherwise.

## Dump identity

- ISO `Gran Turismo (v2.0) [UCES-01245].iso` — 1,177,026,560 bytes, SHA1
  `c84473375e3cd000d8f14c5905e6514389fd6a40`, MD5 `4dc1b4d42e5081616d46538c629f42cb`.
  *(verified: `sha1sum`/`md5sum`/`stat`, 2026-10-09)*
- Volume id `GRANTURSIMO` (sic), publisher `SONY COMPUTER ENTERTAINMENT INC.`, preparer `POLYPHONY DIGITAL
  INC.`, ISO dated 2009-09-09. *(verified: `isoinfo -d`, 2026-10-09)*
- `PARAM.SFO`: DISC_ID `UCES01245`, DISC_VERSION `2.00`, PSP_SYSTEM_VER `5.55`, TITLE `Gran Turismo`.
  *(verified: `strings PARAM.SFO`, 2026-10-09)*
- `UMD_DATA.BIN` = `UCES-01245|7E69EEB8732AE407|0001|G`. *(verified: `cat`, 2026-10-09)*
- Volume serial of EU 2.0 per Nenkai's hub is `274199722` (EU 1.0: `272909914`). Not yet checked
  against our `GT.VOL` header. *(source: <https://nenkai.github.io/gt-modding-hub/builds/gtpsp/>, 2026-10-09)*

## Disc layout

- `PSP_GAME/SYSDIR/EBOOT.BIN` 7,058,320 B — encrypted `~PSP`, module name `PDIAPP`, tag `0xD9160BF0`,
  devkit field `0x05050010`. *(verified: `xxd` header, 2026-10-09)*
- `PSP_GAME/SYSDIR/BOOT.BIN` 7,057,972 B — **all zero bytes**; size equals the decrypted ELF. Useless.
  *(verified: `tr -d '\0' | wc -c` = 0, 2026-10-09)*
- `PSP_GAME/USRDIR/GT.VOL` 1,053,487,104 B — game data volume (assets, Adhoc scripts). Out of scope for
  the code decomp; tool: Nenkai's GTPSPVolTools. *(verified: `isoinfo -l`, 2026-10-09)*
- `PSP_GAME/USRDIR/MODULE/LIBSUPPREACC.PRX` — plain (unencrypted) ELF, Sony lib, `.comment` =
  `GCC: (GNU) 4.0.4+allegrex-2.2.3-psp-2.8.2`. *(verified: `readelf -h`, `strings`, 2026-10-09)*
- `PSP_GAME/USRDIR/MODULE/LIBFONT.PRX`, `PSMF.PRX` — Sony libs with a `~SCE` wrapper; pspdecrypt answers
  "Unknown input file format!". Out of scope. *(verified: ran pspdecrypt, 2026-10-09)*
- `PSP_GAME/SYSDIR/UPDATE/` — firmware updater (`EBOOT.BIN` module `updater`, `DATA.BIN`). Out of scope.
  *(verified: `xxd`, 2026-10-09)*

## Decrypted EBOOT (`PDIAPP`)

- Decrypted ELF: 7,057,972 B, SHA1 `8746583fa8298f6447132988a01b236a8c5994b0`, produced by pspdecrypt
  `c156627db7634d395c380c0a9589130f603307fc` ("Decryption successful for tag D9160BF0 with type 2").
  *(verified: ran it, `sha1sum`, 2026-10-09)*
- ELF type `0xFFA0` (PSP relocatable PRX), machine MIPS, `e_flags 0x10a23001` = MIPS-II, Allegrex mach,
  EABI32, noreorder. Entry `0x10c`. *(verified: `readelf -h`, 2026-10-09)*
- **197 section headers survive, with names** (`.text`, `.rodata`, `.rodata.0001`…`.0027`, `.data`,
  `.bss`, `.bss.0001`…`.0104`, `.ctors`, `.dtors`, `.eh_frame`, `.gcc_except_table`, `.sceStub.text`,
  `.lib.stub`, `.rodata.sceNid`, …). **No `.symtab`** — stripped. *(verified: `readelf -S -W`, 2026-10-09)*
- Segment 0 (RWE): vaddr `0x0`, size `0x4d8488`. Segment 1 (RW): vaddr `0x4d8500`, filesz `0x4e9c`,
  memsz `0x9afac`. Segment 2: PRX relocations, `0x1db5b8` bytes (8 B per entry). *(verified: `readelf -l`, 2026-10-09)*
- `.text` vaddr `0x28`, size `0x4767b4` (4,679,604 B, 1,169,901 words). `.init` at `0x0`, `.fini` at
  `0x4767dc`. *(verified: `readelf -S`, 2026-10-09)*
- `.data` `0x4d8500` (+`0x4e9c`); `.bss` `0x4dd400` (+`0x92cfc`); `.sbss` size 0.
  *(verified: `readelf -S`, 2026-10-09)*
- `.comment` holds three compilers, merged (cannot be attributed per object from this section alone):
  `GCC: (GNU) 4.0.4+allegrex-2.2.3-psp-2.8.2`, `GCC: (GNU) 3.3.6+allegrex-2.2.2-psp-1.15.3.internal`,
  `GCC: (GNU) 3.3.6+allegrex-2.2.2-psp-1.14.0`. Section `.gcc_compiled_long32` present.
  *(verified: `readelf -p .comment`, 2026-10-09)*
- **No `$gp`-relative loads/stores anywhere in `.text`** → built with `-G0`; don't configure a small-data
  base. `sceModuleInfo.gp` field reads `0xce90` (raw, pre-relocation). *(verified: raw-opcode scan +
  rabbitizer scan, both 0 hits, 2026-10-09)*
- `sceModuleInfo` at `0x477530`: name `PDIAPP`, attr `0x0`, exports `0x477244–0x477254`, imports
  `0x47725c–0x47752c`. *(verified: parsed struct, 2026-10-09)*
- C++ with exceptions in part of the code: `.ctors` `0x538` B (334 entries), `.dtors` `0x1c0` B (112),
  `.eh_frame` 48 CIEs / **774 FDEs covering only `0x3233c` B (4.4 %) of `.text`**, first non-init FDE at
  `0x356960`. So `.eh_frame` is *not* a general source of function boundaries.
  *(verified: parsed `.eh_frame`, 2026-10-09)*
- Game-side RTTI is absent: the only typeinfo names are libstdc++/`__cxxabiv1` ones; no `_Z` mangled
  strings. *(verified: `strings | grep`, 2026-10-09)*
- `jr $ra` count in `.text`: 28,218 → order of magnitude ~20–28k functions. *(verified: rabbitizer
  1.16.2 scan, 2026-10-09)*
- VFPU: 2,164 instructions (0.18 % of `.text`); 1,645 of them in `0x300000–0x3fffff`, none past
  `0x400000`. Top ops `lv.q`, `lv.s`, `sv.q`, `mtv`, `vmov.q`. *(verified: rabbitizer `R4000ALLEGREX`
  scan, 2026-10-09)*
- Source-file names left in strings: `sgxsys.c`, `sgxbmg.c`, `sgxres.c`, `sgxsnd.c`, `sgxsndnote.c`,
  `sgxseq.c`, `sgxport.c`, `sgxvo.c`, `sgxconf.c`, `sgxpsp.c` (SGX sound lib), `MMenuGameObjectManager.cpp`,
  `adhoc_system.cpp`. *(verified: `strings`, 2026-10-09)*
- Imports: 36 libraries, 329 functions — e.g. `ThreadManForUser` 43, `sceSasCore` 27, `sceMpeg` 26,
  `sceUtility` 17, `IoFileMgrForUser` 15, `sceNetAdhoc` 14, `sceAtrac3plus` 14, `sceGe_user` 7.
  *(verified: parsed `.lib.stub`, 2026-10-09)*

## Compilers (search + tests, strategy decision input)

- decomp.me `psp-gcc-1.3.1` = `GCC 3.3.3+allegrex-2.2.1-psp-1.3.1`; `psp-gcc-1.7.1` =
  `GCC 3.3.3+allegrex-2.2.2-psp-1.7.1`; both i386 Linux ELF, need `/usr/lib32/ld-linux.so.2` (present).
  *(verified: `strings`, `.ident` of a `-S` compile, 2026-10-09)*
- Library code in GT matching those compilers' libs verbatim (longest relocation-free run ≥ 12 words):
  libgcc 6/126, libc 1/514, libm 0/192, libstdc++ 3/333, libsupc++ 3/131 (1.7.1; 1.3.1 alike). Hits lie
  in `0x35e7a0–0x374388` → SDK library region. *(verified: byte search script, 2026-10-09)*
- `func_0012207C` (11 words, counter clamp) **matches byte for byte** with psp-gcc 1.7.1 `-O2 -G0` and
  `-Os -G0`; not with `-O1`. *(verified: compile → psp-as 1.3.1 → word compare, 2026-10-09)*
- `func_00013DD0` (21 words, ring-buffer index) does **not** match with 1.7.1 or 1.3.1 at
  `-O1/-O2/-Os/-O3`: the game computes `i * 148` with `sll`/`addu`, both compilers emit `mult`; delay
  slots filled differently. *(verified: same harness, 2026-10-09)*
- Game code uses `bnel`/branch-likely, `movz`, `move` as `addu rd, rs, $zero`; `ll`/`sc` refcount loops
  exist at `0x257930`, `0x257a28`. *(verified: rabbitizer disassembly, 2026-10-09)*

## PSPRecomp (strategy B base)

- Commit `f6e7d415c7f447b934cc3865a31eb725f353d659` (2026-08-13), MIT. Framework `src/` + `include/`:
  5,855 lines. VCS profile host layer `profiles/vcs/host`: 30,810 lines (DX12 GPU backend). 226
  `register_hle(lib, nid, …)` entries. *(verified: clone + `wc` + parse, 2026-10-09)*
- **GT's imports with an HLE in PSPRecomp (framework + VCS host): 151/329.** Complete: `sceCtrl` 3/3,
  `sceGe_user` 7/7, `sceDisplay` 4/4, `InterruptManager` 3/3, `sceSuspendForUser` 1/1. Partial:
  `ThreadManForUser` 32/43, `sceMpeg` 22/26, `sceSasCore` 21/27, `sceAudio` 11/12, `IoFileMgrForUser`
  9/15, `sceAtrac3plus` 7/14, `sceUmdUser` 6/9, `SysMemUserForUser` 5/11, `UtilsForUser` 5/8,
  `sceUtility` 4/17, `sceRtc` 4/5, `ModuleMgrForUser` 3/9, `scePower` 2/4, `LoadExecForUser` 1/2,
  `sceWlanDrv` 1/2. None: `sceLibFont` 11, `scePsmf` 13, `sceMp3` 12, `scesupPreAcc` 8,
  `Kernel_Library` 6, `StdioForUser` 3, `sceUsb` 4, `scePspNpDrm_user` 2, `sceImpose` 1, `sceHprm` 1,
  and all networking: `sceNet` 2, `sceNetInet` 2, `sceNetApctl` 6, `sceNetAdhoc` 14, `sceNetAdhocctl`
  11, `sceNetAdhocMatching` 11. *(verified: GT `.lib.stub` NIDs × `register_hle` literals, 2026-10-09)*
- lcs-recomp (the LCS port on PSPRecomp) builds on Linux with Vulkan, SDL2, FFmpeg. *(verified: its
  README, 2026-10-09)*

## Phase 1 tooling (verified 2026-10-09)

- `tools/setup.sh --with-ghidra`: first run ~18 s (543 MB Ghidra zip, SHA256 checked), second run
  0.3 s with everything `up to date` / `cached`. *(verified: ran twice)*
- `.venv` was created with **Python 3.13.15** (uv's pick), not the system 3.14. *(verified: pytest header)*
- `.venv/bin/python -m tools.extract`: real dump → `extracted/EBOOT.elf` with SHA1 `8746583f…94b0`
  in ~2 s; second run 0.04 s (skip path). `GT.VOL` is not extracted. *(verified: ran twice)*
- pytest: 23 passed (incl. the real-dump test), tools coverage 89 %. *(verified: ran it)*
- `rea doctor --provider ghidra` with `GHIDRA_INSTALL_DIR=tools/ghidra/ghidra_12.1.3_PUBLIC` and
  `JAVA_HOME=/usr/lib/jvm/java-27-openjdk`: Ghidra 12.1.3 healthy, Java 27 accepted. *(verified: ran it)*
- **REA 6.2.0 cannot open the EBOOT**: `analyze` → `target_unavailable` "unsupported ELF
  architecture" in 7 ms, also with `e_type` patched to `EXEC`; `elfArchitecture()` in
  `dist/domain/binaryTarget.js` admits only `e_machine` 3/62/40/183. *(verified: ran it + read the source)*
- Ghidra 12.1.3 + ghidra-allegrex v21.4 headless on `extracted/EBOOT.elf`: loader **PSP Executable
  (ELF)**, language **`Allegrex:LE:32:default`**, image base `0x00000000`, **23,208 functions**,
  1,132,862 instructions, VFPU decoded (1,122 matches of `lv.`/`sv.`/`vmov`/`vadd`/`vmul`/`vscl`/`mtv`/
  `mfv`), `adhoc_system.cpp` string at `0x4aea78`, `.text` `0x28` size `0x4767b4`. Total analysis
  154 s; JVM inside the cgroup scope, ~1 GB RSS early in the run. *(verified: analyzeHeadless + `Phase1Probe.java`)*

## Runtime (PPSSPP 1.20.4, verified 2026-10-09)

- **`PDIAPP` load base = `0x08804000`**: "Loadable Segment Copied to 08804000, size 004d8488",
  `.text` at `08804028` (= vaddr `0x28` + base), "Module entry: 0880410c" (= vaddr `0x10c` + base);
  memory block `08804000–08d77500` tagged `ELF/PDIAPP`. So **runtime = ELF vaddr + 0x08804000**.
  *(verified: `PPSSPPHeadless "<iso>" -l --timeout=15 --graphics=software`)*
- In 15 s headless the game boots into MPEG playback (thousands of `sceMpegAvcDecode`/`sceMpegGetAtracAu`
  calls, 55 `scePsmf` calls), opens `disc0:/PSP_GAME/USRDIR/GT.VOL` 98 times, and logs **0 unimplemented
  syscalls**. The 8 `scesupPreAcc` imports start unresolved and are resolved when `LIBSUPPREACC.PRX`
  loads. *(verified: same log)*
- `PPSSPPHeadless` and `PPSSPPSDL` are in Arch's `ppsspp` package; headless takes the ISO directly.
  *(verified: `pacman -Ql ppsspp`, ran it)*
- The headless log with `-l` is ~900k lines for 15 s; write it to disk, never to a terminal, and
  `/tmp` here is a 16 GB **tmpfs (RAM)** — put big logs elsewhere or delete them. *(verified: `df -h /tmp`)*

## Recompiler on GT (Phase 2 exploration, verified 2026-10-09)

- PSPRecomp `f6e7d41` framework builds on Linux with clang + Ninja in ~8 s; `psprecomp_tests` pass.
  *(verified: cmake/ctest)*
- `psp_analyze extracted/EBOOT.elf report.json 0x08804000` (0.7 s): PRX recognised, module `PDIAPP`, gp
  `0x08CE9390`, entry `0x0880410C`, **243,383 relocations, 0 unsupported/invalid** (R_MIPS_32 40,852,
  R_MIPS_26 115,535, HI16 40,516, LO16 46,480), 41,212 function seeds, 882,937 code labels, 11,330
  indirect call sites, 30,834 "unsupported instruction occurrences". *(verified: ran it)*
- Upstream `psp_recomp --auto` never finishes on GT: endless emission (RSS → 5.7 GB in 10 min, nothing
  written). gdb backtrace: `emit_function_source` → `ostringstream` growth. *(verified: gdb)*
- **lcs-recomp `ab91e87`** carries the fixes (import-call `break`, `valid_units`, linear text rewrites):
  `psp_recomp --auto … 0x08804000` → 39 units, 257,198 registered blocks, 329 import wrappers, 117 MB
  of C++ in ~30 s, under the 6 GB cap. Its framework tests pass when built alone (`--target
  psprecomp_tests`); its default build also compiles the LCS profile and fails without
  `vulkan/vulkan.h`. *(verified: ran it)*
- GT's generated units still fail to compile: `static_assert(ScalarRegister < 144u)` with `188`, and
  `local_pc` undeclared in `jr` lowering. *(verified: clang -fsyntax-only on generated_unit_0038)*
- Installed: `vulkan-icd-loader`, `sdl2-compat`, `ffmpeg`, `glslang`, `shaderc`; **missing
  `vulkan-headers`**. *(verified: pacman -Q)*

## gt_native boot (Phase 2a, verified 2026-10-09)

- 30 s headless run: ~82,800 import calls; trace_compare matches **235** calls in order with PPSSPP,
  then `sceGeListEnQueue` (PPSSPP) vs `sceKernelDelayThread` (ours); later a null+8 load at guest pc
  `0x08ADB53C` (unit 0090). *(verified: gt_native --max-seconds 30 --trace, trace_compare)*
- PPSSPP timings GT depends on: `sceUtilityLoadModule` suspends the caller ~25,000 us (4/4 loads);
  `scePowerSetClockFrequency` ~150,000 us; a repeated `sceCtrlReadBufferPositive` waits for the next
  pad sample ("ctrl buffer waited"). *(verified: context-switch deltas in ppsspp.log)*
- GT executes `mfv $zero, 255` (VFPU sync idiom) and `vi2uc.q`; `ll`/`sc`, `madd`, `addi`, `mfic`/
  `mtic` all appear on the boot path. A memset uses a computed jump into an unrolled store run
  (`lui/addiu/subu/jr $at`, func at vaddr `0x332018`). *(verified: run stops + rabbitizer/Ghidra)*
- The C string/memory routines sit together at vaddr `0x331ED0..0x332220`: `0x331ED0` strchrnul-style
  search (returns the NUL, never NULL), `0x331F60` strlen, `0x331FD8` strcmp, `0x332018` memset,
  `0x332170` memcmp (no NUL test, so not strncmp). Runtime = vaddr + `0x08804000`.
  *(verified: Ghidra + llvm-objdump disassembly, 2026-10-10)*
- Of 1,427 untranslated VFPU sites in generated code, 885 decode as INVALID (data analysed as code);
  real gaps left: `lv/sv{l,r}.q` (39), `vcrs.t` (1), and 480 `vsbn.s` of unclear reality.
  *(verified: rabbitizer over rt.unsupported markers)*

- `GT_SCHED_LOG=1 build/gt_native ...` prints one `[sched] A -> B (uid->uid, reason) pc=` line per
  thread switch. Compared with PPSSPP: there, vblank waiters wake on a vblank event ("entered
  vblank", "vblank start waited") and `sceDisplaySetMode` switches threads ("display mode"); in our
  LCS-derived kernel those waits are timed delays, so PDI:00000004/5 resume in a different order and
  the first `sceGeListEnQueue` is not reached at call 235. *(verified: both logs, 2026-10-09)*

## Adhoc bridge — starting map (verified 2026-10-10)

- Strings: `HADHOC` (vaddr `0x493bf0`, also `0x493c9c`, `0x493d2c`, `0x49418c`), `HModule` (dozens,
  from `0x493bf8`), `pdistd` (`0x493d34`), `pdistd::MMusic` (`0x49419c`). `onAdHocMatching*` strings
  (`0x49543c`…) belong to Sony's network ad-hoc matching, not to the Adhoc script VM.
  *(verified: strings -t x on the ELF)*
- `pdistd::MMusic` is passed as an argument (a1) by `FUN_00005980` (vaddr `0x5980`, runtime
  `0x08809980`); `pdistd` by `FUN_000017f8` (`0x17f8`, runtime `0x088057f8`). Both sit at the very start
  of `.text`, where the static initializers listed in `.ctors` (334 entries) live — likely the native
  module/class registrations Adhoc scripts see. *(verified: Ghidra RefsTo.java)*
- **Native registration** (verified 2026-10-10): an initializer creates its module (e.g.
  `FUN_00265b3c(.., "MLocale")`) and registers each script-callable function with
  `FUN_00382fb0(module, "getLanguage", fn)` (42 callers); class methods use `FUN_00383108(class,
  "name", fn)` (96 callers, → `FUN_0038305c`); `FUN_00382e98(module, "name", value)` (67 callers)
  registers constants. Extracted with `tools/ghidra_scripts/AdhocNatives.java` (P-code constants):
  **1,329 registrations, 1,313 distinct functions** (e.g. `MOrganizer` 161, `MSpecDB` 86,
  `MRaceOperator` 58). They seed `config/symbol_addrs.txt` (`tools/adhoc_symbols.py`).
  *(verified: Ghidra decompilation of FUN_00239394 / FUN_00382fb0 / FUN_00383108)*
- Next step: decompile the small natives first (names known), cross-check names with OpenAdhoc.

## Environment (this machine)

- Ghidra `12.1.2` at `/opt/ghidra` (Arch `extra` has nothing newer). ghidra-allegrex v21.4 ships only a
  build for Ghidra `12.1.3`; v21.3 covers up to `12.1`. *(verified: `pacman -Si`, GitHub releases API, 2026-10-09)*
- REA MCP `rea-agents` 6.2.0 connected; Ghidra provider `not_configured` until `GHIDRA_INSTALL_DIR` is set
  (user-scope registration has empty `env`). *(verified: `binary_session`, `claude mcp get rea`, 2026-10-09)*
- Present: `7z`, `isoinfo`, `python3` 3.14, `uv`, `java` 17, `gcc`, `clang`, `cargo`, `ninja`, `cmake`,
  `wine`, `docker`. Absent: PPSSPP, any MIPS binutils, `objdiff-cli`. *(verified: `command -v`, 2026-10-09)*
- 24 cores, 31 GiB RAM. *(verified: `nproc`, `free -h`, 2026-10-09)*

## Commands that work here

- `7z x -o<dir> <iso> -x'!PSP_GAME/USRDIR/GT.VOL'` — extracts the ISO without the 1 GB volume.
  `isoinfo -x` produced empty files here. *(verified: ran both, 2026-10-09)*
- `pspdecrypt -o <out.elf> EBOOT.BIN` — decrypts the game EBOOT (build: `make` in the repo; needs zlib +
  OpenSSL `libcrypto`). *(verified: ran it, 2026-10-09)*

## Adhoc class registration (verified 2026-10-13)

- Script classes register **methods** through `0x08B87108` and **attributes** through `0x08B878E8`,
  as `(class, name string, handler, handler)`. One attribute handler does both get and set. Example:
  MRaceParameter at `0x08847D64..`, names at `0x08C9CC0C..` (.rodata), 31 attributes and 28 methods.
  *(verified: generated code of unit 0008 plus the ELF strings; table in `config/symbol_addrs.txt`)*
- The engine-side RaceParameter is big-endian (be16/be32 fields), consistent with `MRaceParameter::pack`
  sending it online. *(verified: the getters' byte swaps, `src/game/race/race_parameter.cpp`)*
- MCarParameter (initializer `0x08854EB4`): 12 attributes (11 through `0x08B878E8`, `control_type` through `0x08B8D51C`) and 40 methods. The script object's first word
  points to the engine handle (`0x08854678`), the handle's first word to the data, and the data holds
  **three 384-byte car blocks from `+0x38`**, one selected by the signed byte at `+0x08`; the per-car
  accessors (quicktune_*, color, car code) all index with it. A spec struct hangs off `+0x4C8`
  (weight, power, torque, PP). *(verified: the engine accessors the handlers call,
  `src/game/car/car_parameter.cpp`, `include/gt/game/car_parameter.hpp`)*
- MCalendar (initializer `0x08818A04`): 1 attribute (`today`) and 12 methods. The script object holds the
  engine object at `+0x14`; the engine object starts with the day number (`0x08B1B2F4` year/month/day, the
  first day is 2009-10-01) and holds a ring of 100 events of 148 bytes from `+0x14`, with the count at
  `+0x10`. *(verified: the engine functions the handlers call, `src/game/calendar/calendar.cpp`)*
- MSpecDB: 86 static natives (registrar `0x08B86FB0`, handlers `0x0883A3D4..0x08841500`), no attributes. A car or
  course code is a 64-bit key, `(id, table number)` as `(low, high)`: table 0 for cars and 34 for courses
  (`CarCode32ToCarCode64` / `CourseCode32ToCourseCode64`); a car record is 32 bytes (price at `+0x08`,
  year `+0x0C`, category `+0x15`), a course record 28 bytes (condition `+0x14`, max entry `+0x15`); the
  maker table has 96 entries and the tuner table 114 (`getMakerSize` / `getTunerSize` store them). *(verified:
  the engine functions the natives call, `src/game/specdb/spec_db.cpp`)*
- MCarCollection (initializer `0x0881C3E8`): 34 methods. Its engine object (script object `+0x14`) holds the
  record array pointer at `+0x30` (34 bytes per car type: low nibble of byte `i + 2` is how many, high nibble
  flags), two 100-entry lists at `+0x34` and `+0x1C4`, and 30 favourite slots at `+0x354` that are
  CarParameter handles; `getOneCarMax` returns the constant 10. *(verified: the engine functions the natives
  call, `src/game/car/car_collection.cpp`)*
- MRaceOperator (initializer `0x0882506C`): 58 methods and 11 attributes. The script object's `+0x14` is the engine
  handle (`+0x00` data, `+0x04` organizer, `+0x08` lock); the data holds the slots (392 bytes each) at `+0x00`,
  their count at `+0x04`, `window_max` at `+0x0C`, the series (`+0x30`, `+0x34`, 64-bit course codes from `+0x38`),
  the race parameter at `+0x7C` and six 28-byte memory-plan blocks from `+0x90`. *(verified: the engine functions
  the attribute handlers call, `src/game/race/race_operator.cpp`)*
- **Inside a harness case, nested calls run the recompiled originals**, so a decompiled child is not
  exercised by its decompiled parent's cases (a value sabotage of such an engine function went
  undetected). A child gets its own cases when the routes reach it outside the parent's cases (the
  parent runs unsampled and reaches the child's trampoline); a child reached only inside sampled parent
  calls, or never, has none, and stays unregistered. *(verified: sabotage of `get_car_price`)*
