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
