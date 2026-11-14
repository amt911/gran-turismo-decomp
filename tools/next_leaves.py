"""Leaves still to decompile: the functions the recompiler emitted, nothing has taken yet, and that call nothing.

New decompilation starts with leaves (docs/agents/decomp-workflow.md): a function without calls needs no callee to be
decompiled first and its synthetic cases only exercise its own body. A function is a leaf when it has no `jal`, `jalr`,
`j` out of the function (a tail call) and no `jalr` or `jr` through a register other than `ra` (virtual calls, function
pointers, jump tables); branches and `j` inside the function are fine. A tail call counts as a callee for `--ready`.

"Taken" means a registration line (`GT_DECOMP(0x...u`, a batch macro such as `HOT218(0x...u`, active or commented out:
a commented one has a body and only lacks a comparable case) or a `void func_XXXXXXXX(` definition under `src/`; a mention
of an address in a comment, an expression or a call takes nothing. `--skip FILE` adds the addresses other agents hold (one runtime address per line, `#` comments).

`--ready` widens the list to functions whose direct callees are all taken, so a batch can be built bottom-up once the leaves
are gone (22 untaken leaves of 4..40 instructions remained on 2026-11-14).

Usage (from the repository root; reads extracted/EBOOT.elf, recomp/analysis/functions.tsv, recomp/generated, src/):
  .venv/bin/python -m tools.next_leaves [--ready] [--min-insns 4] [--max-insns 40] [--limit 60] [--skip FILE] [--addresses-only]
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from tools.progress import LOAD_BASE

_OP_J, _OP_JAL = 2, 3
_FUNCT_JR, _FUNCT_JALR = 8, 9
_RA = 31
_LOADS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x31}   # lb lh lwl lw lbu lhu lwr lwc1
_STORES = {0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x39}              # sb sh swl sw swr swc1
_BRANCHES = {0x01, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17}  # regimm beq bne blez bgtz and the likely forms
_REGISTRATION = re.compile(r"^[ \t]*(?://[ \t]*)?[A-Z_][A-Z0-9_]*\(\s*0x([0-9A-Fa-f]{8})u?\s*[,)]", re.MULTILINE)
_DEFINITION = re.compile(r"^(?:\[\[maybe_unused\]\] )?void func_([0-9A-F]{8})\(", re.MULTILINE)


@dataclass(frozen=True)
class Leaf:
    entry: int      # ELF vaddr
    insns: int
    loads: int
    stores: int
    branches: int
    calls: tuple[int, ...] = ()   # ELF vaddrs of the direct callees, in order of first call


def has_indirect_jump(words: Sequence[int]) -> bool:
    """A `jalr` or a `jr` through a register other than `ra`: virtual calls, function pointers and jump tables."""
    for word in words:
        if word >> 26 == 0:
            funct = word & 0x3F
            if funct == _FUNCT_JALR or (funct == _FUNCT_JR and (word >> 21) & 0x1F != _RA):
                return True
    return False


def direct_calls(words: Sequence[int], entry: int = 0) -> tuple[int, ...]:
    """ELF vaddrs of the `jal` targets and of the `j` targets outside the function (tail calls; a `j` inside it is control
    flow), each once, in order. The target field of the file is not relocated."""
    end = entry + 4 * len(words)
    targets = [((word & 0x3FFFFFF) << 2, word >> 26) for word in words if word >> 26 in (_OP_J, _OP_JAL)]
    return tuple(dict.fromkeys(target for target, opcode in targets if opcode == _OP_JAL or not entry <= target < end))


def is_leaf(words: Sequence[int], entry: int = 0) -> bool:
    return not has_indirect_jump(words) and not direct_calls(words, entry)


def summarize(entry: int, words: Sequence[int]) -> Leaf:
    opcodes = [word >> 26 for word in words]
    return Leaf(entry=entry, insns=len(words),
                loads=sum(op in _LOADS for op in opcodes),
                stores=sum(op in _STORES for op in opcodes),
                branches=sum(op in _BRANCHES for op in opcodes),
                calls=direct_calls(words, entry))


def taken_addresses(source: str) -> set[int]:
    """Runtime addresses on registration lines (commented-out ones included) and `void func_XXXXXXXX(` definitions of a source."""
    return {int(match, 16) for pattern in (_REGISTRATION, _DEFINITION) for match in pattern.findall(source)}


def read_skip_file(path: Path) -> set[int]:
    """ELF vaddrs of the runtime addresses a file lists (first word of each line; blank lines and `#` lines ignored)."""
    held: set[int] = set()
    for line in path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            held.add(int(line.split()[0], 16) - LOAD_BASE)
    return held


def candidates(code: Mapping[int, bytes], emitted: Iterable[int], taken: Iterable[int], skip: Iterable[int] = (),
               min_insns: int = 4, max_insns: int = 40, ready: bool = False) -> list[Leaf]:
    """The untaken, emitted leaves with min_insns <= instructions <= max_insns, smallest first. With `ready`, also the functions
    without indirect jumps whose direct callees are all taken (decompiled or in a commented registration): bottom-up order."""
    emitted_set, taken_set = set(emitted), set(taken)
    excluded = taken_set | set(skip)
    found = []
    for entry, raw in code.items():
        if entry not in emitted_set or entry in excluded:
            continue
        words = struct.unpack(f"<{len(raw) // 4}I", raw[:len(raw) // 4 * 4])
        if not min_insns <= len(words) <= max_insns or has_indirect_jump(words):
            continue
        calls = direct_calls(words, entry)
        if not calls or (ready and all(callee in taken_set for callee in calls)):
            found.append(summarize(entry, words))
    return sorted(found, key=lambda leaf: (leaf.insns, leaf.entry))


def format_row(leaf: Leaf) -> str:
    row = f"0x{leaf.entry + LOAD_BASE:08X}  {leaf.insns} insns  {leaf.loads} loads  {leaf.stores} stores  {leaf.branches} branches"
    if leaf.calls:
        row += "  calls " + " ".join(f"0x{callee + LOAD_BASE:08X}" for callee in leaf.calls)
    return row


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - reads the dump
    from tools.progress import load_functions
    from tools.shapes import recompiled_functions, split_inside, text_section

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    parser.add_argument("--recompiled", type=Path, default=Path("recomp/generated"))
    parser.add_argument("--min-insns", type=int, default=4)
    parser.add_argument("--max-insns", type=int, default=40)
    parser.add_argument("--limit", type=int, default=60)
    parser.add_argument("--skip", type=Path, help="runtime addresses other agents hold, one per line")
    parser.add_argument("--ready", action="store_true",
                        help="also the functions whose direct callees are all decompiled (bottom-up order), not only leaves")
    parser.add_argument("--addresses-only", action="store_true", help="print only the addresses (for a batch file)")
    args = parser.parse_args(argv)
    units = sorted(args.recompiled.glob("generated_unit_*.cpp"))
    if not units:
        print(f"no generated units in {args.recompiled}: run .venv/bin/python -m tools.recomp", file=sys.stderr)
        return 1
    base, text = text_section(args.elf.read_bytes())
    inside, _ = split_inside(load_functions(args.functions), base, len(text))
    code = {a: text[a - base:a - base + n] for a, n in inside.items()}
    taken = {a - LOAD_BASE for path in sorted(args.src.rglob("*.cpp")) for a in taken_addresses(path.read_text())}
    skip = read_skip_file(args.skip) if args.skip else set()
    found = candidates(code, recompiled_functions(u.read_text() for u in units), taken, skip, args.min_insns, args.max_insns, args.ready)
    print(f"{len(found)} untaken {'ready functions' if args.ready else 'leaves'} of {args.min_insns}..{args.max_insns} instructions", file=sys.stderr)
    for leaf in found[:args.limit]:
        print(f"0x{leaf.entry + LOAD_BASE:08X}" if args.addresses_only else format_row(leaf))
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
