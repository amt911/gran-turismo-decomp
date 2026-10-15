"""Disassemble one function of the EBOOT with the relocations applied and every branch target resolved.

The decomp is written from the asm (docs/agents/decomp-workflow.md), and a branch target counted by hand by `pc + 4 +
(offset << 2)` was once read one word off (FINDINGS: func_08B54FB8 sent a state to bit 7 instead of bit 10). This prints
the targets as absolute runtime addresses, resolves the `lui`/`addiu`/load pairs the ELF relocates to the address they
build, and names each `jal`/`j` target, so the reading never needs the arithmetic.

Usage (from the repository root; reads extracted/EBOOT.elf and recomp/analysis/functions.tsv):
  .venv/bin/python -m tools.disasm 08B54FB8 [08B54F54:12 ...]
An argument is a runtime address (hex, with or without 0x); `ADDR:N` shows N instructions of a place the function
list does not know (a function the recompiler never started at, a tail-call target).
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

import rabbitizer

from tools.family_members import R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, TextImage
from tools.progress import LOAD_BASE, load_functions

_BRANCH = re.compile(r"\. \+ 4 \+ \((-?0x[0-9A-Fa-f]+) << 2\)")


def resolve_branch(text: str, pc: int) -> str:
    """Replace the `. + 4 + (N << 2)` operand of a branch at `pc` by `-> ADDRESS` (the delay slot is at pc + 4)."""
    match = _BRANCH.search(text)
    if not match:
        return text
    offset = int(match[1], 16)
    if offset & 0x8000 and offset > 0:
        offset -= 0x10000
    return f"{text[:match.start()]}-> {pc + 4 + offset * 4 & 0xFFFFFFFF:08X}"


def disassemble(words: Sequence[int], relocs: Mapping[int, int], segments: Sequence[int], address: int) -> list[str]:
    """One line per word of a function at runtime `address`: `PC: WORD  text  [note]`. `relocs` maps a word index to its
    r_info (low byte: type; bits 16-23: the segment the address is relative to) and `segments` holds each segment's
    p_vaddr."""
    rabbitizer.config.regNames_userFpcCsr = False
    high: dict[int, int] = {}  # register -> the relocated high half it holds
    lines = []
    for index, word in enumerate(words):
        pc = address + 4 * index
        instruction = rabbitizer.Instruction(word, vram=pc, category=rabbitizer.InstrCategory.R4000ALLEGREX)
        text = resolve_branch(instruction.disassemble(), pc)
        info = relocs.get(index, 0)
        kind = info & 0xFF
        segment = (info >> 16) & 0xFF
        base = segments[segment] if segment < len(segments) else 0
        note = ""
        if kind == R_MIPS_26:
            note = f"  -> func_{((word & 0x3FFFFFF) << 2) + LOAD_BASE:08X}"
        elif kind == R_MIPS_HI16:
            high[(word >> 16) & 31] = ((word & 0xFFFF) << 16) + base
            note = "  [HI]"
        elif kind == R_MIPS_LO16:
            register = (word >> 21) & 31
            low = word & 0xFFFF
            low -= 0x10000 if low >= 0x8000 else 0
            note = f"  [LO] = 0x{(high[register] + low + LOAD_BASE) & 0xFFFFFFFF:08X}" if register in high else "  [LO]"
        lines.append(f"  {pc:08X}: {word:08X}  {text}{note}")
    return lines


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="tools.disasm", description=__doc__.splitlines()[0])
    parser.add_argument("functions", nargs="+", metavar="ADDR[:N]")
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions_tsv)
    for argument in args.functions:
        text, _, count = argument.partition(":")
        vaddr = int(text, 16) - LOAD_BASE
        size = int(count) * 4 if count else sizes.get(vaddr)
        if size is None:
            print(f"error: {text} is not a known function; give ADDR:N", file=sys.stderr)
            return 1
        words, relocs = image.function(vaddr, size)
        print(f"== func_{vaddr + LOAD_BASE:08X} {len(words)} insns")
        print("\n".join(disassemble(words, relocs, image.segments, vaddr + LOAD_BASE)))
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
