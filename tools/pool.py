"""The pool of reached, still undecompiled functions, with the static facts that decide whether one is worth reading.

A batch starts from `tools.decomp_gate probe` (call counts of the functions no `src/` file registers) and has to cut that
list down to functions the gate can verify: no VFPU instruction (kept out of the decomp), no import stub within a few
calls (most imports block, switch threads or cannot be recorded, so every case is "not comparable"), not defined or
registered anywhere in `src/` already (commented registrations and written-but-unregistered functions included). This
tool prints that table, hottest first; before it the cut was a scratch script that a reboot could wipe.

Usage (from the repository root; reads extracted/EBOOT.elf, recomp/analysis/functions.tsv and the probe logs):
  .venv/bin/python -m tools.pool recomp/analysis/probe_TAG_*.log [--max-insns 300] [--depth 5] [--stubs] [--extra-src FILE ...]

Columns: hits, runtime address, instructions, stores, direct calls, imports (1 when a `jal`/`j` target past the end of
`.text` is reached within --depth calls), indirect (1 when a `jalr` is on the walk: a virtual or member call the static
walk cannot follow), vfpu, float instructions. --stubs appends the import stub addresses reached, which tell a replay-safe
lock or interrupt import (compares fine) from one the harness cannot record (never comparable).
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Callable, Iterable, Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path

from tools.family_members import TextImage, reached_from_logs
from tools.progress import LOAD_BASE, load_functions

_STORE_OPS = frozenset({0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x39})  # sb sh swl sw swr swc1
_VFPU_OPS = frozenset({0x12, 0x18, 0x19, 0x1A, 0x1B, 0x32, 0x34, 0x35, 0x36, 0x37, 0x3A, 0x3C, 0x3D, 0x3E})
_FLOAT_OPS = frozenset({0x11, 0x31, 0x39})  # COP1, lwc1, swc1
_JALR_FUNCTION = 9  # SPECIAL: jalr


@dataclass(frozen=True)
class Row:
    hits: int
    address: int  # runtime address
    instructions: int
    stores: int
    calls: int
    imports: bool
    indirect: bool
    vfpu: int
    floats: int
    stubs: tuple[int, ...] = field(default=())


def done_addresses(source: str) -> set[int]:
    """Runtime addresses that `src/` text already defines (`func_XXXXXXXX(`) or registers (`NAME(0xXXXXXXXXu, `), commented
    registrations and unregistered definitions included: such a function is not pending."""
    # A definition is `void func_XXXXXXXX(`: a mention in a comment or a call to a recompiled function is not one.
    done = {int(m, 16) for m in re.findall(r"\bvoid\s+func_([0-9A-Fa-f]{8})\(", source)}
    done |= {int(m, 16) for m in re.findall(r"\w+\(0x([0-9A-Fa-f]{8})u?\s*,", source)}
    # A shared body registered by address alone: GT_DECOMP_EMPTY(0xXXXXXXXXu), GT_DECOMP_ZERO(...).
    done |= {int(m, 16) for m in re.findall(r"\b[A-Z][A-Z0-9_]*\(\s*0x([0-9A-Fa-f]{8})u?\s*\)", source)}
    return done


def walk_imports(words_of: Callable[[int], Sequence[int] | None], entry: int, depth: int, text_end: int) -> tuple[set[int], bool]:
    """(import stub targets reached, whether a jalr is on the walk) following `jal`/`j` targets up to `depth` calls. Addresses are
    ELF virtual addresses; a target at or past `text_end` is an import stub."""
    stubs: set[int] = set()
    indirect = False
    best: dict[int, int] = {}  # the most depth left any visit had: a later visit with more depth goes further

    def visit(address: int, remaining: int) -> None:
        nonlocal indirect
        if remaining < 0 or best.get(address, -1) >= remaining:
            return
        best[address] = remaining
        words = words_of(address)
        if words is None:
            return
        for word in words:
            operation = word >> 26
            if operation in (2, 3):
                target = (word & 0x3FFFFFF) << 2
                if target >= text_end:
                    stubs.add(target)
                else:
                    visit(target, remaining - 1)
            elif operation == 0 and (word & 0x3F) == _JALR_FUNCTION:
                indirect = True

    visit(entry, depth)
    return stubs, indirect


def describe(words: Sequence[int]) -> tuple[int, int, int, int]:
    """(stores, direct calls, vfpu instructions, float instructions) of a function's words."""
    stores = sum(1 for word in words if word >> 26 in _STORE_OPS)
    calls = sum(1 for word in words if word >> 26 == 3)
    vfpu = sum(1 for word in words if word >> 26 in _VFPU_OPS)
    floats = sum(1 for word in words if word >> 26 in _FLOAT_OPS)
    return stores, calls, vfpu, floats


def build_rows(hits: Mapping[int, int], sizes: Mapping[int, int], words_of: Callable[[int], Sequence[int] | None], done: Iterable[int],
               text_end: int, max_insns: int, depth: int) -> list[Row]:
    """The pool, hottest first: reached functions (ELF address -> hits) that are not done, are known to the function list and
    have at most `max_insns` instructions."""
    skip = set(done)
    rows = []
    for address, count in hits.items():
        if address + LOAD_BASE in skip or address not in sizes:
            continue
        words = words_of(address)
        if words is None or len(words) > max_insns:
            continue
        stores, calls, vfpu, floats = describe(words)
        stubs, indirect = walk_imports(words_of, address, depth, text_end)
        rows.append(Row(count, address + LOAD_BASE, len(words), stores, calls, bool(stubs), indirect, vfpu, floats,
                        tuple(sorted(stub + LOAD_BASE for stub in stubs))))
    rows.sort(key=lambda row: (-row.hits, row.address))
    return rows


def format_row(row: Row, stubs: bool) -> str:
    text = (f"{row.hits} 0x{row.address:08X} {row.instructions} {row.stores} {row.calls} {int(row.imports)} {int(row.indirect)} "
            f"vf{row.vfpu} fp{row.floats}")
    if stubs and row.stubs:
        text += " " + ",".join(f"{stub:X}" for stub in row.stubs)
    return text


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.pool", description=__doc__.splitlines()[0])
    parser.add_argument("logs", nargs="+", type=Path, help="probe logs written by tools.decomp_gate probe")
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    parser.add_argument("--extra-src", type=Path, nargs="*", default=[], help="more source files whose definitions count as done")
    parser.add_argument("--max-insns", type=int, default=300)
    parser.add_argument("--depth", type=int, default=5)
    parser.add_argument("--stubs", action="store_true")
    args = parser.parse_args(argv)
    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions_tsv)
    hits = reached_from_logs(log.read_text(errors="replace") for log in args.logs)
    source = "".join(path.read_text(errors="replace") for path in sorted(args.src.rglob("*.cpp")))
    source += "".join(path.read_text(errors="replace") for path in args.extra_src)

    def words_of(address: int) -> Sequence[int] | None:
        size = sizes.get(address)
        if size is None or address - image.base < 0 or address - image.base + size > len(image.text):
            return None
        return image.function(address, size)[0]

    rows = build_rows(hits, sizes, words_of, done_addresses(source), image.base + len(image.text), args.max_insns, args.depth)
    print("hits addr insns stores calls imports indirect vfpu float")
    for row in rows:
        print(format_row(row, args.stubs))
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
