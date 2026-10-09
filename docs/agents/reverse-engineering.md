# Reverse engineering — REA, Ghidra, PPSSPP, and names

> Part of `AGENTS.md`, kept in its own file so `AGENTS.md` fits the 32 KiB Codex reads by
> default. Its rules bind all the same: `AGENTS.md` lists the hard ones in *Rules by topic* and
> says when to read this file. Edit the rule here, not a copy of it.

## The target you open

Always the **decrypted ELF** (`extracted/EBOOT.elf`, SHA1 `8746583f…94b0`), never the encrypted
`EBOOT.BIN` and never `BOOT.BIN` (zero-filled). Check the hash before trusting any analysis — a
different file gives confident, wrong answers.

## REA (MCP) — not usable on the EBOOT yet

**REA 6.2.0 rejects MIPS ELFs** ("unsupported ELF architecture", whitelist x86/x86_64/ARM/ARM64 —
FINDINGS), so it cannot open `extracted/EBOOT.elf` with any provider. Until a REA release admits
`e_machine` 8, the game binary is analysed with **Ghidra directly** (next section). What follows
applies to whatever REA *can* open, and to the EBOOT the day it can:

- Invoke the **`reverse-engineer-anything`** skill first; it routes the question to the right tool:
  `open_binary` with the absolute path, then focused tools — `search_strings`, `search_procedures`,
  `procedure_pseudo_code`, `procedure_assembly`, `xrefs`, `procedure_callers` / `procedure_callees`,
  `read_bytes`.
- **Requires the Ghidra provider** — see `CLAUDE.md` (registration with `GHIDRA_INSTALL_DIR`) and
  [toolchain](toolchain.md#ghidra--ghidra-allegrex) (ghidra-allegrex for Allegrex/VFPU/PRX). Without
  ghidra-allegrex, treat any VFPU region and any relocation-dependent pointer as unknown.
- **Summary first, then focused follow-ups**; never repeat an identical call. Big results
  (`binary_session` is ~200 KB) are read with `jq` from the saved file, never inline.
- **Every conclusion separates observation, inference and unknown**, and cites the REA Evidence IDs.
  Pseudocode is an analysis artifact, not source.
- **Annotations made in REA** (`set_address_name`, `annotate_native_function`) are session state. What
  is worth keeping goes to `config/symbol_addrs.txt` with its evidence; the REA session is disposable.
- **One open binary per session, `close_binary` when done.** The Ghidra JVM REA starts is outside any
  `systemd-run` scope you create — see `AGENTS.md` § memory cgroup.

## Names from the Adhoc bridge

`config/symbol_addrs.txt` starts from GT's own Adhoc native registrations (each script-callable
function is registered with its name). Regenerate after improving the extractor:

```bash
systemd-run --user --scope -p MemoryHigh=5G -p MemoryMax=6G -p MemorySwapMax=0 -- \
  env JAVA_HOME=/usr/lib/jvm/java-27-openjdk MAXMEM=4G tools/ghidra/ghidra_12.1.3_PUBLIC/support/analyzeHeadless \
  ghidra gt -process EBOOT.elf -noanalysis -readOnly -scriptPath tools/ghidra_scripts \
  -postScript AdhocNatives.java $PWD/recomp/analysis/adhoc_natives.tsv 0x382fb0 0x383108
.venv/bin/python -m tools.adhoc_symbols recomp/analysis/adhoc_natives.tsv > config/symbol_addrs.txt
```

Hand-made names go in the same file with their evidence; a generated name that turns out wrong is
fixed in the generator or overridden there, never in a second list.

## Ghidra directly — the current engine for the EBOOT

Repo-local Ghidra 12.1.3 + ghidra-allegrex v21.4 (`tools/setup.sh --with-ghidra`). Headless import +
analysis + a probe script is in `AGENTS.md` § Commands (~154 s); it picks the **PSP Executable (ELF)**
loader and the **`Allegrex:LE:32:default`** language by itself, finds 23,208 functions and decodes
VFPU (FACTS). The GCC exception analyzer logs ~389 "Error creating LSDA" lines — noise, see FINDINGS.
For long interactive sessions (struct editing, data types), the same Ghidra's GUI (`ghidraRun`). The Ghidra
project (`ghidra/`, gitignored) is an **exclusive resource**: one process holds its lock. Export what
matters (names, types) to the repo's text files; the `.gpr`/`.rep` is never the source of truth.

## Addresses — two spaces, one offset

- **ELF vaddr (base 0)** — what FACTS, `func_XXXXXXXX` names and `config/symbol_addrs.txt` use; it
  is stable whatever base a loader picks.
- **Runtime / guest address** — what PPSSPP, cheat databases, PSPRecomp's guest memory (to verify)
  and Ghidra-with-base-`0x08804000` show:
  `runtime = vaddr + load_base`, with `load_base` = **`0x08804000`** — measured in PPSSPP 1.20.4 on
  2026-10-09 (FACTS § Runtime).
- Always say which space an address is in when you write it down.

## Naming — a name needs evidence

A name is a claim. Acceptable evidence, strongest first:

1. **Import NIDs** — `.rodata.sceNid` + `.lib.stub` resolve to Sony function names through psplibdoc;
   these are exact.
2. **Strings the code uses** — assert messages, log formats, file names (`adhoc_system.cpp`,
   `MMenuGameObjectManager.cpp`, `sgx*.c` are in the binary).
3. **Adhoc bindings** — the native side registers functions the scripts call; OpenAdhoc's script
   source gives their names. *(Hypothesis to confirm on the first binding table found.)*
4. **Decompiled neighbours** — a callee whose role is fixed by its already-decompiled callers.
5. **Runtime observation** in PPSSPP (what changes when it runs).

Format in `config/symbol_addrs.txt` *(planned)*: one symbol per line with a trailing comment holding the
evidence, e.g. `sceKernelDelayThread = 0x004768A0; // NID 0xCEADEB47 (psplibdoc)` (illustrative line,
address not real). Unknowns keep `func_XXXXXXXX` / `D_XXXXXXXX` (ELF vaddr).

## External sources — useful, not authoritative

- **Nenkai's GT Modding Hub** (<https://nenkai.github.io/gt-modding-hub/builds/gtpsp/>): GT PSP is a
  stripped GT5-branch engine (`gt5m`), little-endian formats, single EBOOT, "next to no leftover debug
  functionality".
- **OpenAdhoc** (<https://github.com/Nenkai/OpenAdhoc>): script-side source, 100 % for GT PSP.
- **GTAdhocToolchain** (<https://github.com/Nenkai/GTAdhocToolchain>): Adhoc bytecode v12 compiler and
  disassembler.
- **CWCheat/PPSSPP patches** for UCES01245 exist (hub) — runtime addresses, convert before use.
- **Other LLMs, forums, press** — leads only. The binary decides; a claim enters FACTS after it has been
  checked against our ELF.

## PPSSPP — the runtime oracle

Not installed yet. Use: `PPSSPPHeadless --debugger=<port>` (websocket debugger, breaks at start) or the
GUI with "Allow remote debugger". Feed it the user's ISO from `game_files/`, never a copy inside the
repo. A PPSSPP instance is an exclusive resource (one agent per instance/port). Details of runtime
checks: [runtime verification](runtime-verification.md).
