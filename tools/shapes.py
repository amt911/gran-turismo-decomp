"""Shape clusters of the functions still to decompile: functions whose instructions match once immediates and
jump targets are masked, so one generic implementation (a template, as in src/core/) covers them all.

The most useful rows are the pending functions that share a shape with an already decompiled one: its
template, registered once more per address, may cover them — each still goes through the gate.

Usage (from the repository root):
  .venv/bin/python -m tools.shapes [--elf extracted/EBOOT.elf] [--functions recomp/analysis/functions.tsv]
                                   [--min 3] [--top 40]
"""

from __future__ import annotations

import argparse
import struct
import sys
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path

# MIPS opcodes (bits 31..26) whose low 16 bits are an immediate to mask: loads, stores, ALU-immediate,
# lui. Branches (beq, bne, blez, bgtz, REGIMM, the "likely" forms) keep their offset: control flow must match.
_IMMEDIATE_OPS = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,  # addi..lui
                  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  # lb..lwu
                  0x28, 0x29, 0x2A, 0x2B, 0x2E,                    # sb, sh, swl, sw, swr
                  0x31, 0x35, 0x39, 0x3D}                          # lwc1, ldc1, swc1, sdc1
_JUMP_OPS = {0x02, 0x03}  # j, jal: the target is masked


def normalize(code: bytes) -> tuple[int, ...]:
    """The instruction words with immediates and jump targets masked."""
    out = []
    for (word,) in struct.iter_unpack("<I", code[: len(code) // 4 * 4]):
        op = word >> 26
        if op in _JUMP_OPS:
            word &= 0xFC000000
        elif op in _IMMEDIATE_OPS:
            word &= 0xFFFF0000
        out.append(word)
    return tuple(out)


def clusters(code: Mapping[int, bytes]) -> list[list[int]]:
    """Functions grouped by shape, largest group first (ties: lowest first address)."""
    groups: dict[tuple[int, ...], list[int]] = {}
    for address in sorted(code):
        groups.setdefault(normalize(code[address]), []).append(address)
    return sorted(groups.values(), key=lambda g: (-len(g), g[0]))


@dataclass(frozen=True)
class Row:
    pending: list[int]
    size: int
    done: dict[int, str] = field(default_factory=dict)


def report(groups: list[list[int]], sizes: Mapping[int, int], done: Mapping[int, str], min_pending: int) -> list[Row]:
    """Clusters with at least `min_pending` functions still to decompile, with their decompiled siblings."""
    rows = []
    for group in groups:
        pending = [a for a in group if a not in done]
        if len(pending) >= min_pending:
            rows.append(Row(pending, sizes[group[0]], {a: done[a] for a in group if a in done}))
    return rows


def text_section(elf: bytes) -> tuple[int, bytes]:
    """(address, bytes) of the ELF32 little-endian file's .text section."""
    shoff, = struct.unpack_from("<I", elf, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 46)

    def section(i: int) -> tuple[int, ...]:
        return struct.unpack_from("<10I", elf, shoff + i * shentsize)

    names = section(shstrndx)
    for i in range(shnum):
        s = section(i)
        start = names[4] + s[0]
        if elf[start:elf.index(b"\0", start)] == b".text":
            return s[3], elf[s[4]:s[4] + s[5]]
    raise ValueError("no .text section")


def main(argv: list[str] | None = None) -> int:  # pragma: no cover - reads the dump
    from tools.progress import LOAD_BASE, load_functions
    from tools.decomp_gate import registered_names

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    parser.add_argument("--min", type=int, default=3, help="pending functions a cluster needs to be listed")
    parser.add_argument("--top", type=int, default=40)
    args = parser.parse_args(argv)
    base, text = text_section(args.elf.read_bytes())
    sizes = load_functions(args.functions)
    code = {a: text[a - base:a - base + n] for a, n in sizes.items() if base <= a and a - base + n <= len(text)}
    done: dict[int, str] = {}
    for path in sorted(args.src.rglob("*.cpp")):
        for runtime, name in registered_names(path.read_text()).items():
            done[runtime - LOAD_BASE] = name
    rows = report(clusters(code), sizes, done, args.min)
    print(f"{len(rows)} clusters with >= {args.min} pending functions; "
          f"{sum(len(r.pending) for r in rows)} functions in them; "
          f"{sum(len(r.pending) for r in rows if r.done)} share a shape with a decompiled one")
    for row in sorted(rows, key=lambda r: (not r.done, -len(r.pending) * r.size))[: args.top]:
        sibling = ", ".join(f"{n} @0x{a + LOAD_BASE:08X}" for a, n in list(row.done.items())[:2])
        print(f"{len(row.pending):5d} x {row.size:4d} B  e.g. 0x{row.pending[0] + LOAD_BASE:08X}"
              f"{'  template: ' + sibling if sibling else ''}")
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
