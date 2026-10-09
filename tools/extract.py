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
