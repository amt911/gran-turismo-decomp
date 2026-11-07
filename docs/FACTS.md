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
- MOrganizer (initializer `0x0882C2A8`): 161 methods and 12 attributes. The engine handle's first word points to a
  block of more than 17,000 bytes whose members are objects of their own: `+0x5AC` the session object most queries go
  through, `+0x5B4` the entries' list, `+0x5CC` the camera, `+0x600` the race display, `+0x3F58` the pause object,
  `+0x3F80` the replay buffer, `+0x3F88` the game condition, `+0x3F98` the sound target. *(verified: the engine
  functions the natives call, `src/game/race/organizer.cpp`)*
- MCarDriverParameter (initializer `0x0884E9D4`): 10 methods and 14 attributes. The script object's handle points
  to a 184-byte data: the driver aids are signed bytes at `+0x8D..+0x97` (`manual`, `assist_tcs`, `assist_asm`,
  `steering_assist_type`, `active_steering`, `active_brake_level`, `physics_pro`, `academy_flag`, `assist_4was`), the
  type (0 = human) and the word setPlayer stores and getControllerPort returns (`unk_A0`) are big-endian words at `+0x9C` / `+0xA0` (the `user_number` attribute is the plain word `+0x88`), and the AI level is the byte `+0xAF`.
  *(verified: the engine functions the handlers call, `src/game/car/car_driver_parameter.cpp`)*
- MUnit (initializer registers 28 methods, no attributes): the display units are six consecutive words of `.bss` from
  `0x08D6950C` (velocity `+0x00`: 0 = km/h, 1 = mph; power `+0x04`: index into PS, HP, BHP, ch, kW, CV, PF; torque
  `+0x08`: kgfm, ft-lb, Nm; rpm `+0x0C`; `+0x10` whose role is not established; temperature `+0x14`: K, deg.C,
  deg.F). The engine functions at `0x08B48BFC..0x08B497A0` take that address in a0; the internal units are m/s, PS,
  kgfm and m, and `0x08B49238` sets a whole set of units from a system number 0..13 (a language maps to one through
  the 14-pair table at `0x08C95494`). *(verified: the engine functions the natives call,
  `src/game/unit/unit_system.cpp`; `0x08B49238` is gate-verified for system 1 only; its 14 rows (systems 0..13, others behave as 0) were read from the jump table's stores and the other 13 never ran)*
- **Inside a harness case, nested calls run the recompiled originals**, so a decompiled child is not
  exercised by its decompiled parent's cases (a value sabotage of such an engine function went
  undetected). A child gets its own cases when the routes reach it outside the parent's cases (the
  parent runs unsampled and reaches the child's trampoline); a child reached only inside sampled parent
  calls, or never, has none, and stays unregistered. *(verified: sabotage of `get_car_price`)*

- **Code nothing reaches (2026-10-13, `tools.shapes`):** of the functions Ghidra lists inside `.text`, 2368
  are never emitted by the recompiler — no direct call or known function pointer reaches them — so no
  route can run them and no gate can verify a decompiled version (`[decomp] … no recompiled function at …,
  not installed` when one is registered). They stay out of the shape clusters. Evidence: the
  `runtime.register_function(0x…)` lines of `recomp/generated/` against `recomp/analysis/functions.tsv`.

## Page loads as screen signals (verified 2026-10-19)

- The game opens `disc0:/PSP_GAME/USRDIR/GT.VOL` once per handle (hundreds of `sceIoOpen`s by the main menu) and reads its entries at fixed
  absolute byte offsets (`sceIoLseek`, whence 0). Every page of the Adhoc UI is one entry loaded when the page is entered, so the **seek to
  its offset is a deterministic "this page is loading" event**: the same offsets, in the same order, in two `--decomp off` runs and in
  `--decomp on` (the race route's 12 waits are satisfied one after the other in both). A press 300 vblank waits after a page's seek landed on every
  page of the nine routes (from a press to the next page's seek takes 110-190, ~1250 for the race's own pages); the first load (the main menu,
  `0x1FA48900`) comes ~800 before the menu shows (the Welcome screen runs first, and in `--decomp on` the load comes ~350 earlier than in `off`
  while the screen does not), hence its margin: 1700 until 2026-10-23, 2300 since (under load the Welcome screen outran 1700 and the first press was lost: exit 97 in options, challenge2, manual). *(measured: `GT_IO_LOG=1` seeks with their vblank
  against `GT_FRAME_DUMP` frames, `off` and `on`, 2026-10-19)*
- Page offsets seen: `0x1CF2A980` the Single Player hub (every visit reloads it), `0x1CDB9140` Mode Selection, `0x1D04FC40` Race Options,
  `0x1D058440` the Drift Trial popup, `0x1CD6DB80` Car Selection, `0x1D0332C0` Car Settings, `0x1CD7D200` Track Selection, `0x1D043C80` Track Options,
  `0x1CC98CC0` the Starting Grid / a challenge's Start page, `0x1D19DB00` the race's own pages, `0x1CE62E80` / `0x1CE7D400` / `0x1CDF8540` the
  Driving Challenges map, a block and its list, `0x1CD2FF80` Ad Hoc Mode (and its Share & Trade level), `0x1CEF9300` its "WLAN switch is OFF" notice,
  `0x1D0FE780` the User Manual. Names for five of them have string evidence: guest RAM dumps at each screen hold `projects/gt5m/arcade/GB/TopRoot.gpb` (the
  hub), `DrivingModeRoot.gpb`, `RaceDetailPopup.gpb` and `CourseRoot.gpb` in the string pool near `0x08D62CE0` exactly when the matching seek had just
  happened. The others are named by the screen on the frame dump, not by a string. *(measured: `GT_RAM_DUMP`, `GT_IO_LOG`, frame dumps)*
- Not usable as a signal: GT.VOL offsets below `0x1000000` (the volume's table, read on every lookup), `0x08CF4070` (a monotonic id counter of an event
  queue, `src/game/unsorted/hot_leaves.cpp` func_0897783C), heap pointers (`0x0944CFB0` held a pointer to the page's background image path,
  right for each screen, but a heap address is whatever the allocator gave), and returns to a page that stays resident (the main menu after Ad Hoc Mode loads nothing).
  *(measured: RAM dumps at 20 vblanks of two runs, stable-and-deterministic words only)*

## The user's save and the savedata cipher (verified 2026-10-23)

- A real PSP save of GT (`UCES01245-GAMEDAT`: `GAME.DAT`, `PARAM.SFO`, `ICON0.PNG`, `PIC1.PNG`) is encrypted: `PARAM.SFO`'s
  `SAVEDATA_PARAMS` starts with `0x41`, and `SAVEDATA_FILE_LIST` keeps a 16-byte hash after each file's 13-byte name.
  GT passes a 16-byte key at param `+0x5DC` with `secureVersion` 0 at `+0x5EC`, built with SDK 5.x: the firmware's
  **mode 5**, which uses no console-bound (fuse) key, so any PSP's save decrypts. *(measured: the key logged from the
  savedata HLE; the hash computed by `tools/savedata.py` equals `PARAM.SFO`'s; the decrypted save boots to Day 25)*
- GT also scrambles its own payload (both the encrypted save and the plain save the HLE writes have entropy ~7.9);
  `func_08C4FE98` is a two-generator XOR over it (read from its asm).
- With a save the main menu loads `0x2458A6C0` instead of `0x1FA48900` (the Welcome screen's first-start page), with
  the cursor on Driving Challenges; the Single Player hub opens with its cursor on OK and Mode Selection on the last mode
  (the save's: Drift Trial). The other page offsets are the same as with an empty save. *(measured: `GT_IO_LOG` + frames)*
- Mode Selection's order with the save: Time Trial, Single Race, Drift Trial (from the left).

## Player car and the per-car input record (verified 2026-11-04)

Runtime addresses (ELF vaddr + `0x08804000`). All from `func_0894DD9C` (the physics step, `src/game/unsorted/hot_batch_123.cpp`),
its asm (`python -m tools.disasm 0894DD9C`) and `GT_PROBE` runs of the `race` and `circuit` pad routes (`--decomp off`,
4 cars, 1 human), plus RAM dumps (`GT_RAM_DUMP`) of the same routes. Probe code was a scratch patch of `host/probes.cpp`
(reverted, not committed): it printed the DD9C frame's registers `s2`/`s3` (world, car index) at the entry of `0x08948B78`.

- **One call of `0x0894DD9C(world)` steps all cars** (`world+8` = 4 cars, `world+4` = slots of `0x2100` bytes, car =
  slot + `0x2010`, `car+8` = the car's data, `world+0x10` = the controller). Called through the function table from
  `0x088E1FA4` (virtual slot `0x18` of the object at `s2+0x148`), so a hook on it sees every step (10 471 hits in `race`).
  *(observed: `generated_unit_0019.cpp:10421` `ctx.pc = 0x0894DD9C; return;`)*
- **Per car the mode is `controller->vtable[2](controller, i)`** (`0x08898FF8`): the controller `world+0x10` holds a list
  at `+0xC` (`func_088F8D58(list, i)` -> the car's *driver entry*; byte index at `list+0x24+i`, mask `list+0x2C`); mode 2 if
  the entry's virtual `+0x38` holds, 1 if `+0x60`/`+0x74` holds or the entry byte `+8` is set, else 0. Mode 0 and
  `car+8` byte `+3` != 4 -> `0x08948AE4` (zero record), controller slot `0xC` (= `0x08899094` -> entry virtual `+0x64` =
  `0x08896780`, fills the record from the pad device `entry+0x6C`), **`0x08948B78(record)`**, slot `0x10`, then
  **`0x08952B68(car, record)`**. Mode 1 -> `0x08952A5C`; mode 2 or kind 4 -> `0x089525A0`; no controller -> `0x08952B68` with
  the zero record. *(observed: asm + decompiled C++; `0x08898FF8` semantics read from `hot_leaves.cpp:15285`)*
- **The human car is the one whose driver entry has vtable `0x08CC1F60`** (AI cars: vtable `0x08CC1EA0`). In `race`/`circuit`
  with 4 cars, indices 0..2 were AI (`car+8` byte `+3` = 2, entry vtable `0x08CC1EA0`) and index 3 the player (byte `+3` = 0,
  vtable `0x08CC1F60`, pad device `entry+0x6C` = `0x094168D8`). `0x08948B78` was entered exactly once per step, always for
  index 3 (race: 10 471 of 10 471 steps; circuit: 2 889 of 2 889). The AI cars never reach `0x08948B78` (mode 1).
  *(measured: probe tally; entry/vtable fields read from the RAM dumps)*
- **Replay of the same session reuses the same world, controller, entry and car 3**, and still enters `0x08948B78` once per step
  with a (replayed) non-zero record (`+8` non-zero), so "called" does not separate live from replay. What changes is
  **driver entry byte `+0xA8`**: 0 while driving, 1 once the replay runs (`circuit`: 1 451 steps with `+0xA8` = 0, 1 349 with 1;
  `func_08896780` skips filling the record from the pad device when `+0xA8` is set; RAM dumps at vblank 18 000 / 18 500 show the
  flip together with `entry+0xFC` -> `0xFFFFFFFF`). *(measured; that `+0xA8` means "replay playback" is inferred from the
  timing against the route and the skipped pad fill - no code that sets it was read)*
- **Limits of the predicate below (read before relying on it):** the mask at `list+0x2C` is **not consulted** (it was clear in
  every run); with two or more human-class entries (adhoc, split screen) it is true for each and the mod refuses to inject (never
  observed); and "live vs replay" (`+0xA8`) rests on **a single replay** (the end of the `circuit` route), none of demo/attract
  replays, pause, photo mode or a time-trial ghost.
- **Time Trial has a second human-class entry** (`perf_tt_newyork` with the user's save, 2026-11-04): entry 0 vtable `0x08CC1F60`,
  `+0xA8` = 0, pad device `+0x6C` = `0x094168D8`, `+0xF8` = 1; entry 1 also vtable `0x08CC1F60`, `+0xA8` = 0, but `+0x6C` = 0,
  `+0xF8` = 0, `+0x08` = `0x00010100` (inferred: the lap recorder/ghost - its role is not read from code). In `race` the single
  human entry has `+0x6C` = `0x094168D8`, `+0xF8` = 1. The predicate below therefore also requires `word(entry+0x6C) != 0`; with it
  exactly one car matches in Time Trial (mod check passes there, both `--decomp` modes). *(measured: entry dump per step)*
- **`0x08952B68`'s record is the one `0x08948B78` clamped** - *inferred* from the asm (both calls take `s6` = `sp+0x20`,
  `0x0894DEC8` / `0x08952B68` call at `0x0894DF80`; the decompiled one passes the same `input`), not compared at run time. Consequently
  writing the analog fields after the clamp reaches `0x08952B68` unchanged (confirmed indirectly: the pedals follow the injection).
- **Predicate** (guest memory only): `entry = word(list + 4 + (2*i + int8(list[0x24+i]))*4)` with `list = word(word(world+0x10)+0xC)`;
  player(i) = `word(entry) == 0x08CC1F60 && byte(entry+0xA8) == 0 && word(entry+0x6C) != 0` (the last term added for Time Trial, above) (matched exactly one car per step in `race`, and was 0 for the
  whole replay phase of `circuit`). `0x08948B78` receives only the record (a0), so a hook there needs `world` (a0 of
  `0x0894DD9C`, captured by a pre-hook on it) or the entry (a0 of `0x08896780`).
- **`0x08952B68` is called by a `goto` inside the recompiled unit that contains `0x0894DD9C`** (`generated_unit_0041.cpp:5338`),
  so in `--decomp off` (the default of `gt_native`) a function-table hook on it never fires (`GT_PROBE` hits = 0 for `0x08952B68`,
  `0x08952A5C`, `0x089525A0`); `0x08948B78` is reached through the table in both modes. With `--decomp on` DD9C is the C++ one and
  calls them through the table.
- **Input record at the entry of `0x08948B78`** (before the clamp): in `circuit`, with CROSS (accelerate) held, s16 `+8` was non-zero
  and `+0xA` and byte `+0` were never non-zero (the routes use no analog stick/trigger). This disagrees with the earlier recon's
  "`+0xA` = analog throttle, `+8` = brake (inferred)"; settled below: `+8` is the accelerator, `+0xA` the brake.

### Proving which car a `0x08948B78` call belongs to (Task 6, fix round 2)

The record (`sp_entry - 0x50`) is one stack buffer shared by every car of the `0x0894DD9C` loop, so its address proves nothing about
the car. Per car in mode 0 the step calls controller slot `0xC` = `0x08899094(controller, index, record)`: it fetches the car's driver
entry (`func_088F8D58(controller+0xC, index)`, asm `0x088990B8`) and calls the entry's virtual slot `0x64` = **`0x08896780(entry,
buffer, index)`** (`0x088990D4-0x088990D8`; `a0` = entry, `a2` = the car index), then copies the buffer into the record (`0x0892B5F4`,
`0x08899124`); `0x0894DD9C` then calls `0x08948B78(record)` (`0x0894DEC8`). So "a `0x08896780` call whose entry is the human entry
and whose index is the human car's index, followed by the clamp" proves the clamp's record is the player's. The caller's `s3` holds
the index only in the recompiled `0x0894DD9C` (`0x0894DE98`/`0x0894DEB0` use `s3` as the index); the decompiled one of `--decomp on`
does not keep it in a register, so a register check cannot be used in both modes. `0x08896780` is entered through the table in both
modes (the analog-pedals mod's hook on it fires: its check passes with `--decomp off` and `on`).

### Which thread pumps SDL events (measured 2026-11-04)

`GT_LOG_PUMP_THREAD=1` makes `host/gt_kernel.cpp` print the Linux tid that runs `display_window_pump()` (once); the analog-pedals mod
logs the tid of its `0x0894DD9C` hook with `GT_MOD_PEDALS_LOG=1`. `race` route, `gt_play` headless: `--decomp off` pump tid 525267 =
hook tid 525267; `--decomp on` pump tid 532357 = hook tid 532357. Both the same: mod hooks run on the SDL-pumping thread (the GE worker
is the only other thread and never touches input). The real controller/keyboard path was not driven headlessly (the dummy video
driver has no input source and no key-injection tool is installed); only SDL's state getters ran, with no controller present.

### Which record field is the accelerator and which the brake (measured 2026-11-04, Task 6)

Scratch diagnostic mod (not committed) hooking `0x0894DD9C` and `0x08948B78`, `race` pad prefix of `circuit`, then CROSS held
600 vblanks, SQUARE held 600, `--decomp off`, car `0x093E8B18` (index 3, `word(world+4) + 3*0x2100 + 0x2010`), pedals read at
`word(car+8) + off`:

| Button held | record s16 `+8` | `+0xA` | float `+0x530` | float `+0x538` | `+0x534` |
| --- | --- | --- | --- | --- | --- |
| none | 0 | 0 | 0 | 0 | 0 |
| CROSS (accelerate) | **1** | 0 | **1.0** | 0 | 1.0 |
| SQUARE (brake) | 0 | **1** | 0 | **1.0** | 0 |

So **`+8` = accelerator -> `car->constants+0x530` (-> `+0x534` effective throttle); `+0xA` = brake -> `+0x538`** - the earlier
recon's names for `+0x530` / `+0x538` ("brake" / "throttle") were swapped. A digital press is the value 1 without an analog flag
(`0x08952B68` scales `value << 12` when the flag is clear, `asm 0x08953F48-0x08953F80`). Analog flags in record byte 0, read from
`0x08952B68` (`python -m tools.disasm 08952B68`): **bit `0x02` makes `+8` analog** (raw `0..0x1000`; `0x08953F54-0x08953F60`, then
the analog branch `0x08954064`, which shapes it through `func_08960BE8` with a table chosen by byte0 bits `0x60`) and **bit `0x04`
makes `+0xA` analog** (`0x08954200-0x08954298`: target = `+0xA / 4096`, plus `(1 - x) * const[0x08CDE108]`, ramped into `+0x538`).
Bits `0x08` / `0x10` are the same switches for `+0xC` / `+0xE`.

Exact record -> pedal mapping of the analog path (`python -m tools.disasm 08952B68 08952B68:500`, `08960BE8`; runtime addresses):
- **Accelerator** (`+8`, byte0 bit `0x02`): `0x08953F48-0x08953F60` takes `raw = +8` unscaled when the flag is set (else `value << 12`);
  `0x08953FDC` sends the analog case to `0x08954064`; `x = raw * 2^-12` (`0x08954078-0x0895408C`, constant `0x3980...`); byte0 bits
  `0x60` choose the curve object (`0x08CF2FDC` when both clear, `0x08CF2FCC` for `0x20`, `0x08CF2FBC` for `0x40`;
  `0x08954084-0x089540DC`), evaluated by `func_08960BE8(object, 0)` with `f12 = x`: object type 3 gives `y = c*x^8 + c8*x`
  (`0x08960CC0-0x08960D04`). Runtime memory of `0x08CF2FDC` (read in game): type 3, `+4` = 0, `+8` (c8) = 0, `+0xC` (c) = 1.0, so
  **`y = x^8`**. Then `target = y + (1 - y) * 0.1` (`0x089540E0-0x089540FC`, 0.1 = float at `0x08CDE108`), and the pedal float `+0x530`
  ramps to `target` by the steps in the object at `sp+0x60` (`+0` up, `+4` down; `0x089540FC-0x08954204`, store `0x08954204`). `raw = 0`
  takes the release path (`0x08953FE4`/`0x08954064` -> `0x08954178`), pedal ramps to 0.
- **Brake** (`+0xA`, byte0 bit `0x04`): `0x08954200` tests the flag, `0x0895426C-0x08954280` `x = (+0xA & 0xFFFF) * 2^-12`;
  `x = 0` skips the floor (`0x08954294` -> `0x089542BC`), target 0, the pedal ramps down; else `target = x + (1 - x) * 0.1` (`0x0895429C-0x089542B4`), pedal `+0x538`
  ramps by the steps `+8` up / `+0xC` down (`0x089542BC-0x0895431C`, store `0x0895431C`).
- Consequences: both pedals jump to **at least 0.1** for any non-zero record (a pedal in (0, 0.1) cannot be produced); the inverse for a
  wanted pedal `p >= 0.1` is `x = ((p - 0.1) / 0.9)^(1/8)` (accelerator) or `(p - 0.1) / 0.9` (brake), `raw = round(x * 4096)`.
  Verified in game: with the inverse the player's pedals settle at 0.5 / 0.3 (`tools/mod_pedals_check.sh`).

Measured response before the inverse (fake 0.5 / 0.3 written straight as records): accelerator `0.5^8 = 0.0039` -> `0.1035`, brake
`0.3` -> `0.37`; AI cars untouched (their pedals vary independently of the injected values).

Where a post-`0x08952B68` hook would fire (`GT_PROBE`, `race` route, 230 s, mods off): `0x0894E9C0` (first of the drive chain,
a0 = car) has **hits 0 with `--decomp off`** (in-unit call) and **44 287 with `--decomp on`** (4 cars x 11 071 steps);
`0x08952B68` 0 / 11 071. With `--decomp on` the probes on `0x0894DD9C` / `0x08948B78` show 0 hits because the decompiled
versions replace the probed trampolines, so a probe cannot measure those in `on` mode (mods installed after the decomp
registrations do wrap them).

## Camera and the world-matrix upload (verified 2026-11-05..07, orbit-camera mod)

Runtime addresses (ELF vaddr + `0x08804000`). Evidence: camera spikes 1-2, then the `orbit-camera` mod and `tools/mod_camera_check.sh`
(`chase` route, frames compared with a run without the mod).

- **The GE never receives a view matrix** (VIEW uploads are identity). The game folds view x model on the CPU and uploads the product per
  object as the WORLD matrix (GE cmd `0x3A`, 4 rows x 3 floats), with a perspective projection (`m[11] = -1`) for 3D and an orthographic
  one (`m[11] = 0`) for the HUD. A world matrix of the race carries the model scale (a basis row of length ~100 on the car, not 1).
- `0x08B59508(this)` (GU class method, `this = 0x08D6CE00`, vtable slot `0x08CE0DD8`) is the lazy matrix flush: with `block = this+0x2E0`,
  bit 0 of `read16(block+0xD18)` -> `0x08B59210(this, 0x3A, word(block+0xD00))` (world), bit 1 -> `0x08B59194(this, 0x3E, word(block+0xD04))`
  (projection), bit 2 -> `0x08B59210(this, 0x40, word(block+0xD08))`. ~85 world uploads per physics step in a race.
- **Hook `0x08B59210`, not `0x08B59508`.** The recompiled `0x08B59508` reaches `0x08B59210` by `ctx.pc = 0x08B59210; return;` (generated
  unit 0106, label L_08B5954C), so the runtime runs it **after** a hook on `0x08B59508` returned: an edit-call-restore hook there restores
  the matrix before it is read. Measured: 695 k edits of the world matrix in a race (even all zeros) left every dumped frame byte-identical
  to the run without the mod, and the GE-list cursor `this+0x5C` did not move during `call_original`. `0x08B59210(this, cmd, ptr)` is a
  leaf that reads `ptr` (12 floats) and writes 13 words into the list, so edit-call-restore is complete there; with it the frames orbit
  (`--decomp off`; `on` below).
- **Scene object** `S = 0x09413E00` (this run): the argument a0 of `0x08928A80` (scene refresh, once per physics step; the only a0 seen in a
  race). `word(S+0x1FA4)` is the active camera (S itself in the chase view); its camera-to-world matrix is at `+0x10` (16 floats, row vectors,
  row 3 = eye). The static word `0x08D11F74` held S in a RAM dump (menus) but reads **0 during the race**, so there is no static chain: the
  mod takes S from the refresh's argument at run time.
- Player car position: `word(car+8)+0x10` (x, y, z; Y-up form `(x, z, -y)`), car from the player predicate (docs above: vtable `0x08CC1F60`,
  `+0xA8 == 0`, `+0x6C != 0`). In view space `p[r] = dot(row_r, carpos - eye)`: the car is at about `(0, -0.2, -4.5)` in the chase view.
- **SELECT cycles the camera view** (`chase` route: `race` + three SELECT presses at +2000/+3000/+4000 vblanks after the last page): the third
  press lands on the chase view (car seen from behind, low in the picture, ~(240, 200) of 480x272); the others are bumper/cockpit/hood views.
  `script done at vblank 17821` (~frame 8620 of the dumps, `GT_FRAME_EVERY=30`); chase frames are those from ~vblank 17970 on.
- Orbit sign: `M' = M * T(-p) * R_y(yaw) * T(p)` with `R_y = {c,0,s; 0,1,0; -s,0,c}` (row vectors): **+90 puts the camera on the car's right
  side** (the car is seen from its right, front pointing to the right of the picture), 180 from the front.

## Car body state, wheel contacts and the physics step (verified 2026-11-07, reset-to-track mod)

Runtime addresses (ELF vaddr + `0x08804000`). Evidence: research probe (hook on `0x0894DD9C`, runs e1-e3b, r1-r3), then the `reset-to-track` mod
and `tools/mod_reset_check.sh`. Spec: `docs/superpowers/specs/2026-11-07-reset-to-track-design.md`.
- **`0x0894DD9C(world)` steps every car once per physics step**, 60 per second of game time; on the `race` route (`--decomp off`) that is ~0.48
  steps per vblank (~2 vblanks per step, deterministic but not exactly constant: `GT_PAD` times are vblanks, so a route cannot be written in steps).
  The race starts (the car answers the throttle) at step ~750.
  Player car = `word(world+4) + i*0x2100 + 0x2010` (the orbit-camera predicate: driver entry vtable `0x08CC1F60`, `+0xA8 == 0`, `+0x6C != 0`, exactly one).
  A hook there runs before the step, so what it writes is integrated that same step. It is dispatched in `--decomp off` and `on` alike.
- **Body state `K = word(car+8)`, Z-up** (the physics frame; the renderer's is Y-up): position `K+0x10/14/18`; basis rows `+0x1C/20/24`, `+0x28/2C/30`,
  `+0x34/38/3C` (up vector = the third row, `K+0x3C` = up.z); Euler angles `+0x6FC`, `+0x700` (tilt), `+0x704` (yaw, 1.571 at the `race` start);
  the rows are rebuilt from the angles by `0x0893C3E4` (called from `0x0893E0FC`); velocity `+0x6CC/6D0/6D4`, speed `+0x6C4` (m/s); angular rates
  `+0x6E4/6E8` (Euler) and `+0x6F0/6F4/6F8` (body); byte `+0x74B` is a flag (bit 2 clear in normal driving; its meaning is not known, the mod just requires it clear).
- **Wheel contact records** `word(car+0x10) + 0x38*i` (i = 0..3, four wheels): word `+0x2C` = surface, **0 on asphalt, `0x0300` off the track**
  (the only off-road value seen), recomputed each step. On the `race` route (throttle held from +1500) the car leaves the asphalt at the end of the
  straight and hits the barrier at step ~1830 (pos x ~ -447, speed 38 -> 17 m/s); contacts read `0x300` from there on.
- **Restoring a pose works from the step hook**: writing position, the three Euler angles, the three basis rows and zeroing velocity, speed and all rates
  puts the car back and it drives on normally (~10-step settle transient, rates up to ~0.6 for a few steps); without zeroing the rates the car keeps its
  spin. No racing line, checkpoint list or "recover car" routine was found (only license-test failure and pit logic).
