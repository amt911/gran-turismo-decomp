# Toolchain — what we use, pinned, and the compiler search

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## Rules

- **Every tool is pinned** (release version, PyPI version or git commit) and fetched/built by
  `tools/setup.sh` *(planned)* into gitignored `tools/bin/`, `tools/src/`, `.venv/`. Nothing is
  vendored. Bumping a pin is its own commit, with the reason.
- **Verified, not remembered.** Before this file names a tool, flag or command, it was checked against
  the tool's README/source or run here; the check date goes next to it. What could not be verified is
  marked *unverified* and has no command.
- **Proprietary compilers never enter git** and are only used with written provenance (where it came
  from, its SHA1, which `.comment` string it reproduces).
- **Python tools run in the repo venv** (`uv venv .venv` + `uv pip install -r tools/requirements.txt`,
  *planned*), never in the system interpreter.

## The table (checked 2026-10-09)

### Core — strategy B

| Tool | Pin | Role | Source / note |
| --- | --- | --- | --- |
| pspdecrypt | `c156627db7634d395c380c0a9589130f603307fc` | decrypt `EBOOT.BIN` | <https://github.com/John-K/pspdecrypt> — `make` (clang, zlib, OpenSSL `libcrypto`). **Verified here** |
| 7-Zip | system `7z` | extract the ISO | `isoinfo -x` writes empty files for this ISO (FINDINGS) |
| lcs-recomp (framework) | `ab91e87fe38f3efc5e4443069e7ce6b5a43e615b` | static recompiler Allegrex → C++ (lcs-recomp's copy of PSPRecomp), guest memory, NID registry, HLE registration, kernel HLE base for our host | <https://github.com/elmasas/lcs-recomp> (MIT). Chosen over upstream PSPRecomp because its generic `psp_recomp --auto` works on GT (upstream hangs — FINDINGS). `tools/setup.sh` checks it out, applies `tools/patches/lcs-recomp/*.patch`, builds `psp_analyze`/`psp_recomp`/`psprecomp_tests` only. **Verified here** |
| Vulkan-Headers | `354fab82dbd91d2526a41ee4dbfff1012a623798` (v1.4.357) | Vulkan C headers for `gt_play`'s GE backend (the system has the loader, not always the headers) | <https://github.com/KhronosGroup/Vulkan-Headers> (Apache-2.0/MIT). `tools/setup.sh` checks it out into `tools/src/Vulkan-Headers` (gitignored). **Verified here** |
| PSP fonts (PGF) | PPSSPP `3a31057b7e44270b4d5cef8c31b6559d51802a3b` (v1.20.4), hashes in `config/ppsspp_fonts.sha256` | `assets/flash0/font/*.pgf` for the sceLibFont HLE (`gt_native` and `gt_play`), so playing needs no PPSSPP install | <https://github.com/hrydgard/ppsspp>; `tools/setup.sh` fetches only that folder (sparse, blob-less) into `tools/src/ppsspp-fonts` (gitignored, never committed); same files as the Arch package's `/usr/share/ppsspp/assets/flash0/font`. **Verified here** |
| PSPRecomp (upstream) | not used | origin of the framework; VCS profile HLE coverage numbers in FACTS | <https://github.com/jessicanataliagta/PSPRecomp> (MIT) |
| rabbitizer | 1.16.2 | Allegrex decoding for tools | <https://github.com/Decompollaborate/rabbitizer>; category `R4000ALLEGREX`. **Verified here** (VFPU decoded) |
| m2c | git `main` (pin a commit on first install) | asm → C reading aid | <https://github.com/matt-kempster/m2c>; `-t mipsel-gcc-c`; no VFPU |
| Ghidra | 12.1.3 repo-local (`tools/ghidra/`, SHA256-checked); `/opt/ghidra` 12.1.2 unused | the RE engine for the EBOOT | needs Java ≥ 21: `JAVA_HOME_OVERRIDE` → `java-27-openjdk` in its `launch.properties` (system default stays 17). **Verified here** |
| ghidra-allegrex | v21.4 (for Ghidra **12.1.3**) | Allegrex/VFPU, PRX relocations, PSP calling convention | <https://github.com/kotcrab/ghidra-allegrex> |
| REA | `rea-agents@latest` (6.2.0 at setup) | MCP reverse-engineering front-end | **cannot open MIPS ELFs** (x86/x86_64/ARM/ARM64 only) — not usable on the EBOOT; see [reverse engineering](reverse-engineering.md) |
| PPSSPP | v1.20.4 (not installed) | the original's behaviour: boot-check reference + debugger | <https://github.com/hrydgard/ppsspp>; `PPSSPPHeadless --debugger=<port>` |
| psplibdoc | git (pin on install) | NID → Sony function names | <https://github.com/pspdev/psplibdoc> |
| GTPSPVolTools | git (pin on install) | `GT.VOL` unpack (asset/runtime questions only) | <https://github.com/Nenkai/GTPSPVolTools> |

Other PSPRecomp lineages exist (`sp00nznet/psprecomp`, C output, 12 stars, forks `Sifayne/…`,
`zmbkilla/…`); not evaluated. The `jessicanataliagta` one is chosen because two working PC ports
(VCS, LCS) are built on it.

### Code derived from third-party sources (kept with its licence)

| Our file | Taken from | Licence |
| --- | --- | --- |
| `include/gt/vfpu.hpp` | The per-operation VFPU semantics of lcs-recomp's `tools/codegen_main.cpp` @ `ab91e87`, written as functions over `psprecomp::AllegrexContext` | MIT, © the lcs-recomp authors (notice in the file header) |

### Optional — matching track

| Tool | Pin | Role | Source / note |
| --- | --- | --- | --- |
| psp-gcc 1.7.1 | decomp.me `psp-gcc-1.7.1.tar.gz` | Sony SDK GCC **3.3.3+allegrex-2.2.2-psp-1.7.1** (i386 Linux) | <https://github.com/decompme/compilers/releases> — its `as`/`ld` are empty files |
| psp-gcc 1.3.1 | decomp.me `psp-gcc-1.3.1.tar.gz` | Sony SDK GCC 3.3.3+allegrex-2.2.1-psp-1.3.1, **with working `psp-as`** (binutils 2.14 allegrex-vfpu) | same |
| objdiff | v3.8.2 | per-function diff | <https://github.com/encounter/objdiff>; README lists PSP |
| splat | `splat64[mips]` 0.50.0 | asm/data split | <https://github.com/ethteck/splat>; `platform: psp`; no relocatable ELFs |
| decomp-permuter | git (pin on install) | near-match search | <https://github.com/simonlindholm/decomp-permuter> |

### Ghidra + ghidra-allegrex

The installed Ghidra is 12.1.2 and Arch's `extra` repo has nothing newer; ghidra-allegrex v21.4 ships
only a build for 12.1.3 (v21.3 stops at 12.1), and Ghidra refuses extensions built for another version.
Two ways out, in order of preference:

1. **Repo-local Ghidra 12.1.3** in `tools/ghidra/` (gitignored) with ghidra-allegrex v21.4 installed
   into it, and `GHIDRA_INSTALL_DIR` pointing there (the variable `.mcp.json` passes to REA). No `sudo`,
   nothing system-wide changes.
2. Build ghidra-allegrex from source against `/opt/ghidra` 12.1.2 (Gradle + `GHIDRA_INSTALL_DIR`).

Either way: image base **`0x08804000`** for games (ghidra-allegrex README), which makes Ghidra addresses
equal runtime addresses — see [reverse engineering](reverse-engineering.md#addresses--two-spaces-one-offset).

## Compiler — searched, not found

`.comment` lists three compilers (exact strings in [FACTS](../FACTS.md)), all Sony-SDK builds of GCC
with Allegrex patches: **3.3.6 psp-1.14.0**, **3.3.6 psp-1.15.3.internal**, **4.0.4 psp-2.8.2**.

Searched on 2026-10-09:

- **decomp.me** (`decompme/compilers`) hosts two Sony SDK GCCs, both **older**: `psp-gcc-1.3.1` = GCC
  3.3.3 allegrex-2.2.1, `psp-gcc-1.7.1` = GCC 3.3.3 allegrex-2.2.2 (versions read from the binaries
  with `strings`). Plus `pspsnc_1.2.7503.0` and eleven `mwccpsp_3.0.1_*` builds.
- **GitHub code/repo search and web search** for `allegrex-2.2.2-psp-1.14`, `-1.15`,
  `allegrex-2.2.3-psp-2.8`, `3.3.6+allegrex`, `4.0.4+allegrex`: nothing.
- **pspdev** builds a modern upstream GCC: no use for matching.

Tested on 2026-10-09 (method and numbers in FACTS):

- The runtime libraries shipped with 1.3.1/1.7.1 do **not** appear in GT's EBOOT byte for byte
  (libgcc 6 of ~126 functions, libstdc++ 3 of ~335) → GT was linked against a newer SDK runtime.
- Hand-written C for two small game functions, compiled with 1.7.1 `-G0`: one **matches byte for
  byte** with `-O2` (and `-Os`); the other differs in a way C can't fix (`x * 148` compiled to `mult`
  where the game has shifts/adds, plus different delay-slot filling) → **the game's compiler is close
  to 1.7.1 but not the same**.

**Outcome:** per the user's rule ("search first; if not found, strategy B"), strategy B. The matching
track stays open as optional evidence (see [decomp workflow](decomp-workflow.md#optional-a-matching-check)).
If a 3.3.6 psp-1.14/1.15 or 4.0.4 psp-2.8.2 build ever surfaces with clean provenance, record it here
and re-test.
