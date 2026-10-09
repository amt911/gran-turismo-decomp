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
