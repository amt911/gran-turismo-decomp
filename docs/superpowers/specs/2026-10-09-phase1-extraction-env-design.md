# Phase 1 — reproducible extraction and environment (design)

Approved in chat by the user on 2026-10-09 ("me parece bien, recuerda usar rea").

## Goal

From a clean clone plus the user's ISO in `game_files/`, two commands produce a verified decrypted
ELF and a working analysis environment (REA with Ghidra + ghidra-allegrex), with every tool pinned and
nothing copyrighted or third-party vendored into git. This is groundwork for the decomp (strategy B,
end goal: moddable source); it decompiles nothing.

## Components

### `tools/setup.sh` — idempotent, pinned, repo-local

- `.venv` via `uv venv .venv` + `uv pip install -r tools/requirements.txt` (exact pins: `rabbitizer==1.16.2`,
  `pyelftools`, `pytest`, `pytest-cov`; versions fixed when written).
- pspdecrypt: clone `https://github.com/John-K/pspdecrypt` into `tools/src/pspdecrypt`, check out
  `c156627db7634d395c380c0a9589130f603307fc`, `make`, copy the binary to `tools/bin/pspdecrypt`.
- PSPRecomp: clone `https://github.com/jessicanataliagta/PSPRecomp` into `tools/src/PSPRecomp` at
  `f6e7d415c7f447b934cc3865a31eb725f353d659`. **Not built** (Phase 2).
- `--with-ghidra`: download the official Ghidra 12.1.3 release zip and ghidra-allegrex v21.4
  (`ghidra_12.1.3_PUBLIC_20260825_ghidra-allegrex.zip`) into `tools/ghidra/`, install the extension
  into that Ghidra's `Ghidra/Extensions/`. Exact Ghidra zip URL and its SHA256 are taken from the
  official GitHub release when implementing, and checked after download.
- Re-running skips every step whose output already exists at the pinned version. Any failure stops
  the script with the step name. Writes only under `tools/` and `.venv/` (all gitignored). No `sudo`.

### `tools/extract.py` — Python, TDD

1. Read `config/checksums.sha1` (sha1sum format: `<sha1>  <logical name>`; entries `iso` and
   `eboot.elf`).
2. Find the ISO: `--iso <path>` or the single `*.iso` in `game_files/`; verify its SHA1.
3. Extract with `7z x` into `extracted/iso/`, excluding `PSP_GAME/USRDIR/GT.VOL`.
4. Parse the `~PSP` header of `EBOOT.BIN` (magic, module name, ELF size, tag at `0xD0`) and check it
   against the expected values (`PDIAPP`, 7,057,972, `0xD9160BF0`) before decrypting.
5. Run `tools/bin/pspdecrypt -o extracted/EBOOT.elf …`; verify the ELF SHA1.
6. Idempotent: if `extracted/EBOOT.elf` already has the right SHA1, steps 3–5 are skipped.
7. Errors name what was expected and what was found (hash, magic, size, missing tool → "run
   `tools/setup.sh`"). Exit code non-zero on any mismatch.

Units: `tools/checksums.py` (parse/verify), `tools/psp_header.py` (`~PSP` header parse),
`tools/extract.py` (orchestration, subprocess calls injectable for tests).

### `config/checksums.sha1`

The two hashes already in FACTS: ISO `c84473375e3cd000d8f14c5905e6514389fd6a40`, ELF
`8746583fa8298f6447132988a01b236a8c5994b0`.

### REA (required — "recuerda usar rea")

- `GHIDRA_INSTALL_DIR` for REA points at `tools/ghidra/ghidra_12.1.3_PUBLIC` (absolute path; set in
  `.mcp.json`'s default or the user-scope registration — whichever the approval check shows is in
  effect, recorded in FACTS).
- Acceptance: `binary_session` reports the Ghidra provider available; `open_binary` on
  `extracted/EBOOT.elf` succeeds; `binary_overview` and one `search_strings` (`adhoc_system.cpp`)
  return results that agree with FACTS; `close_binary`. Whether ghidra-allegrex is actually picked up
  by REA's headless analysis (VFPU decoded, PRX relocations applied) is checked and recorded either way.

### PPSSPP

The user installs it (`sudo pacman -S ppsspp`, extra 1.20.4). Then the module load base is measured
in PPSSPP's debugger and recorded in FACTS (expected `0x08804000`, unverified until then). If the
user hasn't installed it when the rest is done, this item stays open in ROADMAP — it does not block
Phase 1's other deliverables.

## Testing

- pytest under TDD for `checksums.py`, `psp_header.py`, `extract.py` with synthetic fixtures only
  (hand-built `~PSP` headers, fake hash files, a fake `7z`/`pspdecrypt` via injected runners).
  Expected values written by hand. Coverage ≥ 80 % over `tools/`.
- One `@pytest.mark.needs_dump` end-to-end test runs the real pipeline when the ISO is present,
  skipped otherwise.
- `setup.sh` is glue: proven by running it twice from a clean state (second run does nothing) and by
  the hashes/versions it prints.
- Every assertion seen failing once (e.g. a wrong expected hash in a fixture).

## Docs touched

AGENTS.md *Commands* (move what now exists out of *(planned)*), FACTS (versions installed, REA result,
load base), ROADMAP (Phase 1 checkboxes), toolchain.md if a pin changes.

## Out of scope

Building PSPRecomp or a GT profile, the differential harness, any decompilation, installing anything
system-wide.
