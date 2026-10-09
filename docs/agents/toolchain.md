# Toolchain — what we use, pinned, and the compiler question

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

| Tool | Pin | Role | Source / note |
| --- | --- | --- | --- |
| pspdecrypt | `c156627db7634d395c380c0a9589130f603307fc` | decrypt `EBOOT.BIN` | <https://github.com/John-K/pspdecrypt> — `make` (clang, zlib, OpenSSL `libcrypto`). **Verified here.** |
| 7-Zip | system `7z` | extract the ISO | `isoinfo -x` writes empty files for this ISO (FINDINGS) |
| splat | `splat64[mips]` 0.50.0 | split asm/data, generate linker script | <https://github.com/ethteck/splat>; `platform: psp` since 0.24.0. **Does not take relocatable ELFs** — split raw segments |
| rabbitizer | 1.16.2 | Allegrex decoding (used by splat/spimdisasm) | <https://github.com/Decompollaborate/rabbitizer>; category `R4000ALLEGREX`. **Verified here** (VFPU decoded) |
| spimdisasm | 1.42.4 | disassembler behind splat | <https://github.com/Decompollaborate/spimdisasm> |
| objdiff | v3.8.2 | per-function diff + progress report | <https://github.com/encounter/objdiff>; README lists PSP. VFPU rendering *unverified* |
| m2c | git `main` (pin a commit on first install) | asm → C draft | <https://github.com/matt-kempster/m2c>; `-t mipsel-gcc-c`; no VFPU |
| decomp-permuter | git (pin on install) | brute-force near-matches | <https://github.com/simonlindholm/decomp-permuter>; *flags for PSP unverified* |
| Allegrex binutils | pspdev release `v20261001` | assembler/linker with VFPU (`psp-as`, `psp-ld`) | <https://github.com/pspdev/pspdev/releases> (`pspdev-ubuntu-latest-x86_64.tar.gz`). Only binutils are relevant: pspdev's GCC is modern and will not match |
| Ghidra | 12.1.x (12.1.2 installed at `/opt/ghidra`) | interactive RE + REA's native engine | REA requires 12.1.x |
| ghidra-allegrex | v21.4 (for Ghidra **12.1.3**) | Allegrex/VFPU, PRX relocations, PSP calling convention | <https://github.com/kotcrab/ghidra-allegrex> |
| REA | `rea-agents@latest` (6.2.0 at setup) | MCP reverse-engineering front-end | see [reverse engineering](reverse-engineering.md) |
| PPSSPP | v1.20.4 (not installed) | runtime oracle + debugger | <https://github.com/hrydgard/ppsspp>; `PPSSPPHeadless --debugger=<port>` |
| psplibdoc | git (pin on install) | NID → Sony function names | <https://github.com/pspdev/psplibdoc> |
| GTPSPVolTools | git (pin on install) | `GT.VOL` unpack (runtime/asset questions only) | <https://github.com/Nenkai/GTPSPVolTools> |

### Ghidra + ghidra-allegrex

The installed Ghidra is 12.1.2 and Arch's `extra` repo has nothing newer; ghidra-allegrex v21.4 ships
only a build for 12.1.3 (v21.3 stops at 12.1), and Ghidra refuses extensions built for another version.
Two ways out, in order of preference:

1. **Repo-local Ghidra 12.1.3** in `tools/ghidra/` (gitignored) with ghidra-allegrex v21.4 installed
   into it, and `GHIDRA_INSTALL_DIR` pointing there (the variable `.mcp.json` passes to REA). No `sudo`,
   nothing system-wide changes.
2. Build ghidra-allegrex from source against `/opt/ghidra` 12.1.2 (Gradle + `GHIDRA_INSTALL_DIR`).

Either way: image base **`0x08804000`** for games (ghidra-allegrex README), which makes Ghidra addresses
equal runtime addresses; splat/linking works at **base 0** — see
[reverse engineering](reverse-engineering.md#addresses--two-spaces-one-offset).

## Compiler — the blocking question

`.comment` lists three compilers (exact strings in [FACTS](../FACTS.md)), all Sony-SDK builds of GCC
with Allegrex patches: **3.3.6 psp-1.14.0**, **3.3.6 psp-1.15.3.internal**, **4.0.4 psp-2.8.2**. Sony's
own `LIBSUPPREACC.PRX` from the same disc was built with the 4.0.4 one. Which compiler built **the game
code** is unknown — the merged `.comment` cannot say.

What is and isn't available (2026-10-09):

- **decomp.me** PSP platform offers `psp-gcc-1.3.1` (provenance unknown), `pspsnc_1.2.7503.0` (SN
  Systems) and `mwccpsp_3.0.1_*` (Metrowerks, 11 builds). **No Sony GCC 3.3.6/4.0.4 build** is listed.
  Its assembler is `mips-ps2-decompals-as -march=gs464`, so VFPU functions likely won't assemble there.
- **pspdev**'s GCC is a modern upstream GCC: wrong compiler for matching, fine for binutils.
- Other PSP decomps (sotn-decomp `pspeu`, mhp2g, ygo-tf-5, ts2psp) used **mwccpsp**; none used Sony GCC.
  There is no precedent to copy.

Plan for Phase 2 (each step is cheap and tells us something):

1. **Fingerprint without a compiler.** Allegrex-only instructions (`ext`, `ins`, `min`, `max`, `seb`,
   `seh`, `wsbh`, `clz`), prologue/epilogue shape, delay-slot filling, `long32`/EABI argument registers
   — compare the library ranges (where FDEs exist, from `0x356960`) against the game ranges.
2. **Test-compile leaf functions** on decomp.me with every PSP compiler it offers, to rule them in or
   out. A 100 % on several unrelated functions is evidence; one is luck.
3. **Look for the real thing** — whether Sony's Allegrex GCC patches were published as GPL source (GCC
   is GPL; *unverified*). A compiler rebuilt from published source has clean provenance.
4. **Decide** with the user (see `AGENTS.md` § Strategy) if none of the above reproduces the codegen.

Write each result in FACTS (what was tried, on which function, what came out) so nobody re-runs it.
