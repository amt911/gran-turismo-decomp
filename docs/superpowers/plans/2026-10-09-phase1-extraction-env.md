# Phase 1 — Reproducible Extraction and Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** From a clean clone plus the user's ISO in `game_files/`, `tools/setup.sh` and `tools/extract.py` produce a hash-verified `extracted/EBOOT.elf` and a REA + Ghidra + ghidra-allegrex environment that opens it.

**Architecture:** Three small Python units under TDD (`checksums`, `psp_header`, `extract`) plus one idempotent bash glue script (`setup.sh`) that fetches pinned tools into gitignored `tools/` and `.venv/`. External programs (`7z`, `pspdecrypt`) are called through an injectable runner so tests never need game data.

**Tech Stack:** Python 3.14 + uv venv, pytest 9.1.1, pytest-cov 7.1.0, pyelftools 0.33, rabbitizer 1.16.2; bash; pspdecrypt (C++/clang); Ghidra 12.1.3 + ghidra-allegrex v21.4; REA MCP.

**Spec:** `docs/superpowers/specs/2026-10-09-phase1-extraction-env-design.md`

## Global Constraints

- Nothing copyrighted in git: no ISO/EBOOT/ELF bytes, no extracted files, no fixtures cut from the dump. Test fixtures are synthetic.
- Expected hashes: ISO `c84473375e3cd000d8f14c5905e6514389fd6a40`, decrypted ELF `8746583fa8298f6447132988a01b236a8c5994b0`.
- Expected `~PSP` header: module `PDIAPP`, ELF size `7057972`, tag `0xD9160BF0` (at offset `0xD0`).
- Pins: pspdecrypt `c156627db7634d395c380c0a9589130f603307fc`; PSPRecomp `f6e7d415c7f447b934cc3865a31eb725f353d659`; Ghidra `ghidra_12.1.3_PUBLIC_20260817.zip` SHA256 `93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54`; ghidra-allegrex `ghidra_12.1.3_PUBLIC_20260825_ghidra-allegrex.zip` SHA256 `62e829fa9e6ed6f2bd343763fb339a3f8812b02a4e0ed5a9409b14007b408243`.
- `setup.sh` writes only under `tools/` and `.venv/`; no `sudo`.
- Ghidra 12.1.x requires Java ≥ 21; this machine's default is 17 (`java-27-openjdk` is also installed). Never change the system default; point only the local Ghidra / REA at a JDK ≥ 21.
- Heavy jobs (Ghidra/REA analysis) follow `AGENTS.md` § memory cgroup; pytest here is single-process and small.
- Commits in English, Conventional Commits, ending with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`. Never push without the user asking.

## Review Focus

- **The real ISO filename has spaces, parentheses and brackets** (`Gran Turismo (v2.0) [UCES-01245].iso`) — commands must receive it as one argv element; tested in Task 3 (`test_extract_handles_path_with_spaces_and_brackets`).
- **Uppercase extension `.ISO`** — `find_iso` must accept it; tested in Task 3.
- **A stale `extracted/EBOOT.elf` with the wrong hash** (interrupted earlier run) — must be regenerated, not trusted; tested in Task 3.
- **`config/checksums.sha1` missing an entry** — clear error naming the key, not a `KeyError` traceback; tested in Task 3.
- **Running against an already-decrypted or wrong file as EBOOT** — header check must say "bad magic", before pspdecrypt runs; tested in Task 2 and Task 3.

---

### Task 1: Python scaffolding + `checksums` module

**Files:**

- Create: `tools/requirements.txt`, `pytest.ini`, `.coveragerc`, `tools/__init__.py`, `tools/tests/__init__.py`, `config/checksums.sha1`, `tools/checksums.py`
- Test: `tools/tests/test_checksums.py`

**Interfaces:**

- Produces: `class ChecksumError(Exception)`; `load_checksums(path: Path) -> dict[str, str]` (name → lowercase sha1); `sha1_of(path: Path) -> str`; `verify(path: Path, expected: str, label: str) -> None` (raises `ChecksumError`).

- [ ] **Step 1: Scaffolding files**

`tools/requirements.txt`:

```text
rabbitizer==1.16.2
pyelftools==0.33
pytest==9.1.1
pytest-cov==7.1.0
```

`pytest.ini`:

```ini
[pytest]
testpaths = tools/tests
pythonpath = .
addopts = --strict-markers
markers =
    needs_dump: needs the user's ISO in game_files/ and tools/bin/pspdecrypt; skipped otherwise
```

`.coveragerc`:

```ini
[run]
source = tools
omit = tools/tests/*
```

`tools/__init__.py` and `tools/tests/__init__.py`: empty files.

`config/checksums.sha1`:

```text
# Expected SHA1 of the base dump (UCES-01245 v2.00) and of the decrypted EBOOT. See docs/FACTS.md.
c84473375e3cd000d8f14c5905e6514389fd6a40  iso
8746583fa8298f6447132988a01b236a8c5994b0  eboot.elf
```

Create the venv: `uv venv .venv && uv pip install --python .venv/bin/python -r tools/requirements.txt`

- [ ] **Step 2: Write the failing tests**

`tools/tests/test_checksums.py`:

```python
from pathlib import Path

import pytest

from tools.checksums import ChecksumError, load_checksums, sha1_of, verify

SHA1_ABC = "a9993e364706816aba3e25717850c26c9cd0d89d"  # FIPS 180 test vector for b"abc"
SHA1_EMPTY = "da39a3ee5e6b4b0d3255bfef95601890afd80709"


def test_load_checksums_reads_sha1sum_format_and_skips_comments(tmp_path: Path):
    f = tmp_path / "c.sha1"
    f.write_text(f"# comment\n\n{SHA1_ABC.upper()}  iso\n{SHA1_EMPTY}  eboot.elf\n")
    assert load_checksums(f) == {"iso": SHA1_ABC, "eboot.elf": SHA1_EMPTY}


def test_load_checksums_rejects_malformed_line_with_line_number(tmp_path: Path):
    f = tmp_path / "c.sha1"
    f.write_text(f"{SHA1_ABC}  iso\nnot-a-hash  eboot.elf\n")
    with pytest.raises(ChecksumError, match=r"c\.sha1:2"):
        load_checksums(f)


def test_sha1_of_hashes_file_contents(tmp_path: Path):
    f = tmp_path / "x"
    f.write_bytes(b"abc")
    assert sha1_of(f) == SHA1_ABC


def test_verify_passes_on_match(tmp_path: Path):
    f = tmp_path / "x"
    f.write_bytes(b"")
    verify(f, SHA1_EMPTY, "eboot.elf")


def test_verify_reports_expected_and_actual_on_mismatch(tmp_path: Path):
    f = tmp_path / "x"
    f.write_bytes(b"abc")
    with pytest.raises(ChecksumError) as e:
        verify(f, SHA1_EMPTY, "iso")
    assert SHA1_EMPTY in str(e.value) and SHA1_ABC in str(e.value) and "iso" in str(e.value)
```

- [ ] **Step 3: Run, expect failure**

Run: `.venv/bin/pytest tools/tests/test_checksums.py -v`
Expected: collection error `ModuleNotFoundError: No module named 'tools.checksums'`.

- [ ] **Step 4: Implement `tools/checksums.py`**

```python
"""Read and verify the project's expected hashes (config/checksums.sha1, sha1sum format)."""

from __future__ import annotations

import hashlib
from pathlib import Path

_HEX = set("0123456789abcdef")


class ChecksumError(Exception):
    """A hash file is malformed or a file does not have the expected hash."""


def load_checksums(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        digest = parts[0].lower()
        if len(parts) != 2 or len(digest) != 40 or not set(digest) <= _HEX:
            raise ChecksumError(f"{path}:{lineno}: expected '<sha1>  <name>', got {raw!r}")
        result[parts[1].strip()] = digest
    return result


def sha1_of(path: Path, chunk: int = 1 << 20) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        while block := f.read(chunk):
            h.update(block)
    return h.hexdigest()


def verify(path: Path, expected: str, label: str) -> None:
    actual = sha1_of(path)
    if actual != expected:
        raise ChecksumError(f"{label}: SHA1 mismatch for {path}: expected {expected}, got {actual}")
```

- [ ] **Step 5: Run, expect pass; see one assertion fail on purpose**

Run: `.venv/bin/pytest tools/tests/test_checksums.py -v` → 5 passed.
Temporarily change `SHA1_ABC`'s last digit, rerun, confirm `test_sha1_of_hashes_file_contents` fails, revert.

- [ ] **Step 6: Commit**

```bash
git add tools/requirements.txt pytest.ini .coveragerc tools/__init__.py tools/tests/__init__.py config/checksums.sha1 tools/checksums.py tools/tests/test_checksums.py
git commit -m "feat(tools): checksum file parsing and verification"
```

---

### Task 2: `psp_header` module

**Files:**

- Create: `tools/psp_header.py`
- Test: `tools/tests/test_psp_header.py`

**Interfaces:**

- Produces: `HEADER_SIZE = 0x150`; `@dataclass(frozen=True) class PspHeader(module_name: str, elf_size: int, psp_size: int, tag: int)`; `class PspHeaderError(Exception)`; `parse_psp_header(data: bytes) -> PspHeader`.
- Layout (verified on our EBOOT, FACTS): magic `~PSP` at 0, module name NUL-terminated at `0x0A` (28 bytes), `elf_size` u32 LE at `0x28`, `psp_size` u32 LE at `0x2C`, tag u32 LE at `0xD0`.

- [ ] **Step 1: Write the failing tests**

`tools/tests/test_psp_header.py`:

```python
import struct

import pytest

from tools.psp_header import HEADER_SIZE, PspHeader, PspHeaderError, parse_psp_header


def make_header(name: bytes = b"PDIAPP", elf_size: int = 7057972, psp_size: int = 7058320,
                tag: int = 0xD9160BF0, magic: bytes = b"~PSP") -> bytes:
    h = bytearray(HEADER_SIZE)
    h[0:4] = magic
    h[0x0A:0x0A + len(name)] = name
    struct.pack_into("<II", h, 0x28, elf_size, psp_size)
    struct.pack_into("<I", h, 0xD0, tag)
    return bytes(h)


def test_parses_fields_of_gt_like_header():
    assert parse_psp_header(make_header()) == PspHeader("PDIAPP", 7057972, 7058320, 0xD9160BF0)


def test_rejects_wrong_magic_and_hints_at_decrypted_file():
    with pytest.raises(PspHeaderError, match="magic"):
        parse_psp_header(make_header(magic=b"\x7fELF"))


def test_rejects_short_input():
    with pytest.raises(PspHeaderError, match="too short"):
        parse_psp_header(b"~PSP" + bytes(16))


def test_module_name_stops_at_nul():
    assert parse_psp_header(make_header(name=b"AB\0CD")).module_name == "AB"
```

- [ ] **Step 2: Run, expect failure**

Run: `.venv/bin/pytest tools/tests/test_psp_header.py -v` → `ModuleNotFoundError: No module named 'tools.psp_header'`.

- [ ] **Step 3: Implement `tools/psp_header.py`**

```python
"""Parse the plain-text part of an encrypted PSP executable header (~PSP)."""

from __future__ import annotations

import struct
from dataclasses import dataclass

PSP_MAGIC = b"~PSP"
HEADER_SIZE = 0x150


class PspHeaderError(Exception):
    """The data is not a ~PSP header."""


@dataclass(frozen=True)
class PspHeader:
    module_name: str
    elf_size: int
    psp_size: int
    tag: int


def parse_psp_header(data: bytes) -> PspHeader:
    if len(data) < HEADER_SIZE:
        raise PspHeaderError(f"header too short: {len(data)} bytes, need {HEADER_SIZE:#x}")
    if data[:4] != PSP_MAGIC:
        raise PspHeaderError(
            f"bad magic {data[:4]!r}, expected {PSP_MAGIC!r} (already decrypted, or not an EBOOT?)"
        )
    name = data[0x0A:0x26].split(b"\0", 1)[0].decode("ascii", errors="replace")
    elf_size, psp_size = struct.unpack_from("<II", data, 0x28)
    (tag,) = struct.unpack_from("<I", data, 0xD0)
    return PspHeader(name, elf_size, psp_size, tag)
```

- [ ] **Step 4: Run, expect pass; break one expectation once**

Run: `.venv/bin/pytest tools/tests/test_psp_header.py -v` → 4 passed. Change the expected tag in `test_parses_fields_of_gt_like_header` to `0xD9160AF0`, see it fail, revert.

- [ ] **Step 5: Commit**

```bash
git add tools/psp_header.py tools/tests/test_psp_header.py
git commit -m "feat(tools): parse ~PSP executable header"
```

---

### Task 3: `extract` pipeline

**Files:**

- Create: `tools/extract.py`
- Test: `tools/tests/test_extract.py`, `tools/tests/test_extract_real.py`

**Interfaces:**

- Consumes: Task 1 (`load_checksums`, `sha1_of`, `verify`, `ChecksumError`), Task 2 (`parse_psp_header`, `HEADER_SIZE`, `PspHeaderError`).
- Produces: `class ExtractError(Exception)`; `Runner = Callable[[Sequence[str]], None]`; `find_iso(game_files: Path, explicit: Path | None) -> Path`; `check_eboot_header(eboot: Path) -> None`; `extract(iso: Path, out_dir: Path, checksums: dict[str, str], tool_dir: Path, runner: Runner = run) -> Path` (returns the ELF path); `main(argv: Sequence[str] | None = None) -> int`. CLI: `.venv/bin/python -m tools.extract [--iso PATH] [--out DIR]`.

- [ ] **Step 1: Write the failing unit tests**

`tools/tests/test_extract.py`:

```python
from pathlib import Path

import pytest

from tools.checksums import ChecksumError
from tools.extract import ExtractError, extract, find_iso
from tools.tests.test_psp_header import make_header

SHA1_ABC = "a9993e364706816aba3e25717850c26c9cd0d89d"    # fake ISO content b"abc"
SHA1_EMPTY = "da39a3ee5e6b4b0d3255bfef95601890afd80709"  # fake ELF content b""
SUMS = {"iso": SHA1_ABC, "eboot.elf": SHA1_EMPTY}


class FakeTools:
    """Stands in for 7z and pspdecrypt; records every command."""

    def __init__(self, header: bytes = make_header(), elf_bytes: bytes = b""):
        self.header, self.elf_bytes, self.calls = header, elf_bytes, []

    def __call__(self, cmd):
        self.calls.append(list(cmd))
        if cmd[0] == "7z":
            out = Path(next(a for a in cmd if a.startswith("-o"))[2:])
            eboot = out / "PSP_GAME/SYSDIR/EBOOT.BIN"
            eboot.parent.mkdir(parents=True, exist_ok=True)
            eboot.write_bytes(self.header)
        else:
            Path(cmd[cmd.index("-o") + 1]).write_bytes(self.elf_bytes)


@pytest.fixture
def env(tmp_path: Path):
    iso = tmp_path / "game.iso"
    iso.write_bytes(b"abc")
    tools = tmp_path / "bin"
    tools.mkdir()
    (tools / "pspdecrypt").write_text("")
    return iso, tmp_path / "extracted", tools


def test_extract_happy_path_runs_7z_without_gtvol_then_pspdecrypt(env):
    iso, out, tools = env
    fake = FakeTools()
    elf = extract(iso, out, SUMS, tools, runner=fake)
    assert elf == out / "EBOOT.elf"
    assert fake.calls[0][:2] == ["7z", "x"] and "-x!PSP_GAME/USRDIR/GT.VOL" in fake.calls[0]
    assert fake.calls[1] == [str(tools / "pspdecrypt"), "-o", str(elf),
                             str(out / "iso/PSP_GAME/SYSDIR/EBOOT.BIN")]


def test_extract_skips_everything_when_elf_already_verified(env):
    iso, out, tools = env
    out.mkdir()
    (out / "EBOOT.elf").write_bytes(b"")

    def must_not_run(cmd):
        raise AssertionError(f"ran {cmd}")

    assert extract(iso, out, SUMS, tools, runner=must_not_run) == out / "EBOOT.elf"


def test_extract_regenerates_stale_elf_with_wrong_hash(env):
    iso, out, tools = env
    out.mkdir()
    (out / "EBOOT.elf").write_bytes(b"stale")
    fake = FakeTools()
    extract(iso, out, SUMS, tools, runner=fake)
    assert len(fake.calls) == 2 and (out / "EBOOT.elf").read_bytes() == b""


def test_extract_rejects_wrong_iso_before_running_anything(env):
    iso, out, tools = env
    iso.write_bytes(b"abd")
    fake = FakeTools()
    with pytest.raises(ChecksumError, match=SHA1_ABC):
        extract(iso, out, SUMS, tools, runner=fake)
    assert fake.calls == []


def test_extract_rejects_unexpected_module_before_decrypting(env):
    iso, out, tools = env
    fake = FakeTools(header=make_header(name=b"OTHER"))
    with pytest.raises(ExtractError, match="PDIAPP"):
        extract(iso, out, SUMS, tools, runner=fake)
    assert len(fake.calls) == 1


def test_extract_rejects_already_decrypted_eboot(env):
    iso, out, tools = env
    fake = FakeTools(header=make_header(magic=b"\x7fELF"))
    with pytest.raises(ExtractError, match="magic"):
        extract(iso, out, SUMS, tools, runner=fake)


def test_extract_rejects_wrong_elf_hash(env):
    iso, out, tools = env
    with pytest.raises(ChecksumError, match="eboot.elf"):
        extract(iso, out, SUMS, tools, runner=FakeTools(elf_bytes=b"x"))


def test_extract_requires_pspdecrypt_and_points_to_setup(env):
    iso, out, tools = env
    (tools / "pspdecrypt").unlink()
    with pytest.raises(ExtractError, match="tools/setup.sh"):
        extract(iso, out, SUMS, tools, runner=FakeTools())


def test_extract_names_missing_checksum_entry(env):
    iso, out, tools = env
    with pytest.raises(ExtractError, match="'eboot.elf'"):
        extract(iso, out, {"iso": SHA1_ABC}, tools, runner=FakeTools())


def test_extract_handles_path_with_spaces_and_brackets(env, tmp_path: Path):
    _, out, tools = env
    iso = tmp_path / "Gran Turismo (v2.0) [UCES-01245].iso"
    iso.write_bytes(b"abc")
    fake = FakeTools()
    extract(iso, out, SUMS, tools, runner=fake)
    assert str(iso) in fake.calls[0]


def test_find_iso_picks_single_iso_any_case(tmp_path: Path):
    (tmp_path / "GAME.ISO").write_bytes(b"")
    (tmp_path / "notes.txt").write_text("")
    assert find_iso(tmp_path, None) == tmp_path / "GAME.ISO"


def test_find_iso_refuses_zero_or_several(tmp_path: Path):
    with pytest.raises(ExtractError, match="found 0"):
        find_iso(tmp_path, None)
    (tmp_path / "a.iso").write_bytes(b"")
    (tmp_path / "b.iso").write_bytes(b"")
    with pytest.raises(ExtractError, match="found 2"):
        find_iso(tmp_path, None)


def test_find_iso_explicit_path_must_exist(tmp_path: Path):
    with pytest.raises(ExtractError, match="not found"):
        find_iso(tmp_path, tmp_path / "missing.iso")
```

- [ ] **Step 2: Run, expect failure**

Run: `.venv/bin/pytest tools/tests/test_extract.py -v` → `ModuleNotFoundError: No module named 'tools.extract'`.

- [ ] **Step 3: Implement `tools/extract.py`**

```python
"""Verify the user's ISO, extract it (without GT.VOL), decrypt EBOOT.BIN and verify the ELF.

Usage: .venv/bin/python -m tools.extract [--iso PATH] [--out DIR]
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

from tools.checksums import ChecksumError, load_checksums, sha1_of, verify
from tools.psp_header import HEADER_SIZE, PspHeaderError, parse_psp_header

REPO = Path(__file__).resolve().parent.parent
EXPECTED_MODULE = "PDIAPP"
EXPECTED_ELF_SIZE = 7_057_972
EXPECTED_TAG = 0xD9160BF0
EBOOT_IN_ISO = Path("PSP_GAME/SYSDIR/EBOOT.BIN")
EXCLUDED_FROM_ISO = "PSP_GAME/USRDIR/GT.VOL"
REQUIRED_SUMS = ("iso", "eboot.elf")

Runner = Callable[[Sequence[str]], None]


class ExtractError(Exception):
    """The inputs or tools are not what the pipeline expects."""


def run(cmd: Sequence[str]) -> None:
    subprocess.run(list(cmd), check=True, stdout=subprocess.DEVNULL)


def find_iso(game_files: Path, explicit: Path | None) -> Path:
    if explicit is not None:
        if not explicit.is_file():
            raise ExtractError(f"ISO not found: {explicit}")
        return explicit
    isos = sorted(p for p in game_files.glob("*") if p.is_file() and p.suffix.lower() == ".iso")
    if len(isos) != 1:
        names = ", ".join(p.name for p in isos) or "none"
        raise ExtractError(f"expected exactly one .iso in {game_files}, found {len(isos)} ({names}); pass --iso")
    return isos[0]


def check_eboot_header(eboot: Path) -> None:
    with eboot.open("rb") as f:
        try:
            hdr = parse_psp_header(f.read(HEADER_SIZE))
        except PspHeaderError as e:
            raise ExtractError(f"{eboot}: {e}") from e
    problems = []
    if hdr.module_name != EXPECTED_MODULE:
        problems.append(f"module {hdr.module_name!r}, expected {EXPECTED_MODULE!r}")
    if hdr.elf_size != EXPECTED_ELF_SIZE:
        problems.append(f"ELF size {hdr.elf_size}, expected {EXPECTED_ELF_SIZE}")
    if hdr.tag != EXPECTED_TAG:
        problems.append(f"tag {hdr.tag:#010x}, expected {EXPECTED_TAG:#010x}")
    if problems:
        raise ExtractError(f"{eboot}: unexpected ~PSP header: " + "; ".join(problems))


def extract(iso: Path, out_dir: Path, checksums: dict[str, str], tool_dir: Path,
            runner: Runner = run) -> Path:
    for key in REQUIRED_SUMS:
        if key not in checksums:
            raise ExtractError(f"config/checksums.sha1 has no entry for {key!r}")
    elf = out_dir / "EBOOT.elf"
    if elf.is_file() and sha1_of(elf) == checksums["eboot.elf"]:
        return elf
    verify(iso, checksums["iso"], "iso")
    pspdecrypt = tool_dir / "pspdecrypt"
    if not pspdecrypt.is_file():
        raise ExtractError(f"{pspdecrypt} not found: run tools/setup.sh first")
    iso_dir = out_dir / "iso"
    iso_dir.mkdir(parents=True, exist_ok=True)
    runner(["7z", "x", "-y", f"-o{iso_dir}", str(iso), f"-x!{EXCLUDED_FROM_ISO}"])
    eboot = iso_dir / EBOOT_IN_ISO
    if not eboot.is_file():
        raise ExtractError(f"7z did not produce {eboot}")
    check_eboot_header(eboot)
    runner([str(pspdecrypt), "-o", str(elf), str(eboot)])
    if not elf.is_file():
        raise ExtractError(f"pspdecrypt did not produce {elf}")
    verify(elf, checksums["eboot.elf"], "eboot.elf")
    return elf


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--iso", type=Path, help="ISO path (default: the single .iso in game_files/)")
    parser.add_argument("--out", type=Path, default=REPO / "extracted", help="output dir (default: extracted/)")
    args = parser.parse_args(argv)
    try:
        sums = load_checksums(REPO / "config/checksums.sha1")
        iso = find_iso(REPO / "game_files", args.iso)
        elf = extract(iso, args.out, sums, REPO / "tools/bin")
    except (ExtractError, ChecksumError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as e:
        print(f"error: {e.cmd[0]} exited with status {e.returncode}", file=sys.stderr)
        return 1
    except FileNotFoundError as e:
        print(f"error: {e.filename} not found (is 7z installed?)", file=sys.stderr)
        return 1
    print(f"ok: {elf} (sha1 {sums['eboot.elf']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run unit tests, expect pass; break one expectation once**

Run: `.venv/bin/pytest tools/tests/test_extract.py -v` → 13 passed. Change `EXPECTED_TAG` in a test-local copy (e.g. make `FakeTools` use `tag=0xD9160AF0` in the happy-path test), see it fail with "tag", revert.

- [ ] **Step 5: Write the real-dump test**

`tools/tests/test_extract_real.py`:

```python
from pathlib import Path

import pytest

from tools.checksums import sha1_of
from tools.extract import REPO, main

ELF_SHA1 = "8746583fa8298f6447132988a01b236a8c5994b0"
HAVE_DUMP = any(p.suffix.lower() == ".iso" for p in (REPO / "game_files").glob("*")) and \
    (REPO / "tools/bin/pspdecrypt").is_file()


@pytest.mark.needs_dump
@pytest.mark.skipif(not HAVE_DUMP, reason="needs the user's ISO in game_files/ and tools/bin/pspdecrypt")
def test_real_pipeline_produces_verified_elf(tmp_path: Path):
    assert main(["--out", str(tmp_path)]) == 0
    assert sha1_of(tmp_path / "EBOOT.elf") == ELF_SHA1
    assert not (tmp_path / "iso/PSP_GAME/USRDIR/GT.VOL").exists()
```

- [ ] **Step 6: Coverage**

Run: `.venv/bin/pytest --cov --cov-report=term-missing --cov-fail-under=80`
Expected: all pass (`test_extract_real` skipped until Task 4 builds pspdecrypt), total coverage ≥ 80 %.

- [ ] **Step 7: Commit**

```bash
git add tools/extract.py tools/tests/test_extract.py tools/tests/test_extract_real.py
git commit -m "feat(tools): hash-checked ISO extraction and EBOOT decryption"
```

---

### Task 4: `tools/setup.sh`

**Files:**

- Create: `tools/setup.sh` (mode 755)

**Interfaces:**

- Produces: `.venv/` (Task 1 requirements), `tools/bin/pspdecrypt`, `tools/src/pspdecrypt`, `tools/src/PSPRecomp` (unbuilt), and with `--with-ghidra`: `tools/ghidra/ghidra_12.1.3_PUBLIC` with ghidra-allegrex in `Ghidra/Extensions/` and `JAVA_HOME_OVERRIDE` set to a JDK ≥ 21. Prints the `GHIDRA_INSTALL_DIR` and `JAVA_HOME` to use.

- [ ] **Step 1: Write the script**

```bash
#!/usr/bin/env bash
# Fetch and build the pinned tools for gran-turismo-decomp.
# Idempotent; writes only under tools/ and .venv/; never uses sudo.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$REPO/tools/src"
BIN="$REPO/tools/bin"
GHIDRA_DIR="$REPO/tools/ghidra"

PSPDECRYPT_URL=https://github.com/John-K/pspdecrypt.git
PSPDECRYPT_REV=c156627db7634d395c380c0a9589130f603307fc
PSPRECOMP_URL=https://github.com/jessicanataliagta/PSPRecomp.git
PSPRECOMP_REV=f6e7d415c7f447b934cc3865a31eb725f353d659
GHIDRA_URL=https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.3_build/ghidra_12.1.3_PUBLIC_20260817.zip
GHIDRA_SHA256=93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54
GHIDRA_HOME="$GHIDRA_DIR/ghidra_12.1.3_PUBLIC"
ALLEGREX_URL=https://github.com/kotcrab/ghidra-allegrex/releases/download/v21.4/ghidra_12.1.3_PUBLIC_20260825_ghidra-allegrex.zip
ALLEGREX_SHA256=62e829fa9e6ed6f2bd343763fb339a3f8812b02a4e0ed5a9409b14007b408243

with_ghidra=0
for arg in "$@"; do
  case "$arg" in
    --with-ghidra) with_ghidra=1 ;;
    -h|--help) echo "usage: tools/setup.sh [--with-ghidra]"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

step() { printf '==> %s\n' "$*"; }
need() { command -v "$1" >/dev/null || { echo "missing required command: $1" >&2; exit 1; }; }

# checkout URL REV DIR — shallow fetch of exactly REV; no-op if DIR is already at REV
checkout() {
  local url=$1 rev=$2 dir=$3
  if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD)" = "$rev" ]; then
    echo "    up to date"; return 0
  fi
  rm -rf "$dir"; mkdir -p "$dir"
  git -C "$dir" init --quiet
  git -C "$dir" fetch --quiet --depth 1 "$url" "$rev"
  git -C "$dir" checkout --quiet FETCH_HEAD
}

# fetch_verified URL SHA256 DEST — download once, refuse a wrong hash
fetch_verified() {
  local url=$1 sum=$2 dest=$3
  if [ -f "$dest" ] && echo "$sum  $dest" | sha256sum -c --status; then
    echo "    cached $(basename "$dest")"; return 0
  fi
  curl -fL --retry 3 -o "$dest.part" "$url"
  if ! echo "$sum  $dest.part" | sha256sum -c --status; then
    rm -f "$dest.part"; echo "SHA256 mismatch for $url" >&2; exit 1
  fi
  mv "$dest.part" "$dest"
}

need git
mkdir -p "$SRC" "$BIN"

step "python venv (.venv) from tools/requirements.txt"
need uv
[ -x "$REPO/.venv/bin/python" ] || uv venv --quiet "$REPO/.venv"
uv pip install --quiet --python "$REPO/.venv/bin/python" -r "$REPO/tools/requirements.txt"

step "pspdecrypt @ ${PSPDECRYPT_REV:0:7}"
need make; need clang++
checkout "$PSPDECRYPT_URL" "$PSPDECRYPT_REV" "$SRC/pspdecrypt"
if [ ! -x "$BIN/pspdecrypt" ] || [ "$(cat "$BIN/.pspdecrypt.rev" 2>/dev/null)" != "$PSPDECRYPT_REV" ]; then
  make -C "$SRC/pspdecrypt" -j8 >/dev/null
  install -m 755 "$SRC/pspdecrypt/pspdecrypt" "$BIN/pspdecrypt"
  echo "$PSPDECRYPT_REV" > "$BIN/.pspdecrypt.rev"
fi

step "PSPRecomp @ ${PSPRECOMP_REV:0:7} (source only; built in Phase 2)"
checkout "$PSPRECOMP_URL" "$PSPRECOMP_REV" "$SRC/PSPRecomp"

if [ "$with_ghidra" = 1 ]; then
  step "Ghidra 12.1.3 + ghidra-allegrex v21.4 (repo-local)"
  need curl; need unzip; need sha256sum
  mkdir -p "$GHIDRA_DIR/downloads"
  fetch_verified "$GHIDRA_URL" "$GHIDRA_SHA256" "$GHIDRA_DIR/downloads/ghidra.zip"
  fetch_verified "$ALLEGREX_URL" "$ALLEGREX_SHA256" "$GHIDRA_DIR/downloads/ghidra-allegrex.zip"
  [ -d "$GHIDRA_HOME" ] || unzip -q "$GHIDRA_DIR/downloads/ghidra.zip" -d "$GHIDRA_DIR"
  ext="$GHIDRA_HOME/Ghidra/Extensions"
  [ -d "$ext/ghidra-allegrex" ] || unzip -q "$GHIDRA_DIR/downloads/ghidra-allegrex.zip" -d "$ext"
  # Ghidra 12.1 needs Java >= 21; pick the newest JDK >= 21 under /usr/lib/jvm without touching the system default.
  jdk=""
  for d in $(ls -d /usr/lib/jvm/java-*-openjdk 2>/dev/null | sort -V -r); do
    v=$(basename "$d" | sed -E 's/java-([0-9]+)-openjdk/\1/')
    if [ "$v" -ge 21 ] && [ -x "$d/bin/java" ]; then jdk=$d; break; fi
  done
  [ -n "$jdk" ] || { echo "no JDK >= 21 found under /usr/lib/jvm (install e.g. jdk21-openjdk)" >&2; exit 1; }
  sed -i "s|^JAVA_HOME_OVERRIDE=.*|JAVA_HOME_OVERRIDE=$jdk|" "$GHIDRA_HOME/support/launch.properties"
  echo "    GHIDRA_INSTALL_DIR=$GHIDRA_HOME"
  echo "    JAVA_HOME=$jdk"
fi

step "done"
```

- [ ] **Step 2: Check the zip layouts the script assumes**

Run: `unzip -l tools/ghidra/downloads/ghidra-allegrex.zip | head` (after a first `--with-ghidra` run, or download it by hand to a temp dir first).
Expected: entries under a top-level `ghidra-allegrex/` directory. If the top-level name differs, change the `[ -d "$ext/ghidra-allegrex" ]` check to that name and record it in `docs/FINDINGS.md`. Same check for `ghidra_12.1.3_PUBLIC/` in the Ghidra zip.

- [ ] **Step 3: Run it twice; the second run must do nothing**

Run: `chmod +x tools/setup.sh && tools/setup.sh --with-ghidra` then `tools/setup.sh --with-ghidra` again.
Expected: first run builds/downloads; second prints `up to date` / `cached` everywhere, and `tools/bin/pspdecrypt --help | head -1` prints `Usage: … pspdecrypt [OPTION]... [FILE]`. `git status --short` shows only `tools/setup.sh` (everything else ignored).

- [ ] **Step 4: Commit**

```bash
git add tools/setup.sh
git commit -m "feat(tools): pinned, idempotent tool setup (pspdecrypt, PSPRecomp, local Ghidra)"
```

---

### Task 5: Real extraction, REA acceptance, docs

**Files:**

- Modify: `.mcp.json`, `AGENTS.md` (*Commands*), `docs/FACTS.md`, `docs/ROADMAP.md`, `docs/FINDINGS.md` (only if something cost time)

**Interfaces:**

- Consumes: Tasks 1–4.

- [ ] **Step 1: Real pipeline**

Run: `.venv/bin/python -m tools.extract` → `ok: …/extracted/EBOOT.elf (sha1 8746583f…94b0)`. Run it again → same line, instant (skip path).
Run: `.venv/bin/pytest --cov --cov-fail-under=80` → all pass, `test_real_pipeline_produces_verified_elf` **passed** (not skipped).

- [ ] **Step 2: Point REA at the local Ghidra**

Set `.mcp.json` `rea.env` to `GHIDRA_INSTALL_DIR: "${GHIDRA_INSTALL_DIR:-<absolute path printed by setup.sh>}"` and `JAVA_HOME: "${JAVA_HOME_GHIDRA:-<JDK path printed by setup.sh>}"`. Ask the user to restart Claude Code and approve the project server.

- [ ] **Step 3: REA acceptance (after restart)**

Using the `reverse-engineer-anything` skill and REA tools (loaded in one `ToolSearch` batch):

1. `binary_session` → read the saved JSON with `jq '.result.analysis_provider_candidates[] | {p:.provider.id, a:.availability.status}'`; Ghidra must be `available`. If it is still `not_configured`, the user-scope registration is winning: tell the user the exact `claude mcp remove/add -s user -e …` command from `CLAUDE.md`, and stop here until they run it.
2. `open_binary` with the absolute path of `extracted/EBOOT.elf`.
3. `binary_overview` → segment sizes agree with FACTS (segment 0 size `0x4d8488`).
4. `search_strings` pattern `adhoc_system.cpp` → at least one hit.
5. `procedure_assembly` on a function in the VFPU range (e.g. the first procedure at or after `0x300000` containing `lv.q`) → record whether VFPU mnemonics decode (ghidra-allegrex active) or show as bad data.
6. `close_binary`.

- [ ] **Step 4: Docs**

- `AGENTS.md` *Commands*: replace the planned `tools/setup.sh` / `tools/extract.sh` lines with the real ones (`tools/setup.sh [--with-ghidra]`, `.venv/bin/python -m tools.extract`, `.venv/bin/pytest --cov --cov-fail-under=80`), each marked verified with the date.
- `docs/FACTS.md`: versions installed, the JDK used, REA results of Step 3 (with Evidence IDs), which registration (project/user) ended up in effect.
- `docs/ROADMAP.md`: tick Phase 1 items done; PPSSPP item stays open until Task 6.
- Run `npx --yes markdownlint-cli "*.md" "docs/**/*.md"` → clean.

- [ ] **Step 5: Commit**

```bash
git add .mcp.json AGENTS.md docs/FACTS.md docs/ROADMAP.md docs/FINDINGS.md
git commit -m "docs: phase 1 tooling verified end to end, REA opens the EBOOT"
```

---

### Task 6: PPSSPP load base (needs the user)

**Files:**

- Modify: `docs/FACTS.md`, `docs/agents/reverse-engineering.md` (*Addresses*), `docs/ROADMAP.md`

- [ ] **Step 1: User installs PPSSPP**

Ask the user to run `sudo pacman -S ppsspp` (extra, 1.20.4). Verify with `pacman -Q ppsspp`.

- [ ] **Step 2: Measure the load base**

With the user: boot the ISO from `game_files/` in PPSSPP, open *Debug → Disassembly* (or the remote debugger), and read the start address of module `PDIAPP` (or the address of the entry, ELF vaddr `0x10c`). Load base = observed entry − `0x10c`.

- [ ] **Step 3: Record and commit**

Write the measured base in FACTS (with how it was read), replace "expected `0x08804000`" in `reverse-engineering.md` with the measured value, tick the ROADMAP item.

```bash
git add docs/FACTS.md docs/agents/reverse-engineering.md docs/ROADMAP.md
git commit -m "docs: measured PDIAPP load base in PPSSPP"
```
