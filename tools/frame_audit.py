"""Check that each decompiled body allocates the same stack frame as the original function.

The differential gate compares results and memory side effects, not where on the stack a function keeps its
locals, so a body whose `GuestFrame frame(ctx, N)` differs from the original's `addiu $sp, $sp, -N` still
passes it (FINDINGS, 2026-10-29). This audit reads the size from the ELF and reports every body that disagrees.

    .venv/bin/python -m tools.frame_audit [FILE.cpp ...]     # default: every src/**/*.cpp
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

from tools.family_members import TextImage
from tools.progress import LOAD_BASE, load_functions

# The prologue allocates within the first few instructions; looking further would pick up the frame of a
# neighbour when functions.tsv merges several functions into one entry.
PROLOGUE_WORDS = 12

_BODY = re.compile(r"^void func_([0-9A-F]{8})\(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx\) \{(.*)$",
                   re.MULTILINE)
_FRAME = re.compile(r"GuestFrame frame\(ctx, (0x[0-9A-F]+)u\)")


def declared_frames(source: str) -> dict[int, int]:
    """Runtime address → size of the first GuestFrame of each one-line body that has one."""
    frames: dict[int, int] = {}
    for match in _BODY.finditer(source):
        frame = _FRAME.search(match.group(2))
        if frame:
            frames[int(match.group(1), 16)] = int(frame.group(1), 16)
    return frames


def asm_frame(words: Sequence[int]) -> int | None:
    """Size of the first `addiu $sp, $sp, -N` in the prologue, or None for a leaf without a frame."""
    for word in words[:PROLOGUE_WORDS]:
        if word >> 16 == 0x27BD and word & 0x8000:
            return 0x10000 - (word & 0xFFFF)
    return None


def mismatches(declared: dict[int, int],
               words_of: Callable[[int], Sequence[int] | None]) -> list[tuple[int, int, int]]:
    """(address, original size, body size) for every body whose frame differs from a frame the original has."""
    wrong = []
    for address, size in sorted(declared.items()):
        words = words_of(address)
        original = asm_frame(words) if words else None
        if original is not None and original != size:
            wrong.append((address, original, size))
    return wrong


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.frame_audit", description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions_tsv)

    def words_of(address: int) -> Sequence[int] | None:
        size = sizes.get(address - LOAD_BASE)
        return image.function(address - LOAD_BASE, size)[0] if size else None

    files = args.files or sorted(Path("src").rglob("*.cpp"))
    total = 0
    for path in files:
        for address, original, size in mismatches(declared_frames(path.read_text()), words_of):
            print(f"{path}: func_{address:08X} asm 0x{original:X} body 0x{size:X}")
            total += 1
    print(f"frame audit: {len(files)} files, {total} wrong")
    return 1 if total else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
