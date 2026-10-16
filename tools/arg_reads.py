"""Argument registers a callee reads that the decompiled call does not pass.

`call_set(rt, ctx, 0xADDRu, {a, b})` sets a0 and a1; a callee that also reads a2 sees whatever the C++ context holds,
which is the value of an earlier, different call (FINDINGS batch 93: four `scan_block` members and `func_08896780` faulted on
every case for that reason, and one more bug sat on a path no route runs). This lists, per call with a literal address in a
batch file, the argument registers (a0-a3, t0-t3) the callee reads before writing them that the call does not pass. The scan
is by address order over the whole callee (not a flow analysis), so a register first used on a path that writes it later in
the text is reported too: each line is a place to read the callee's head, not a verdict.

Usage (from the repository root): .venv/bin/python -m tools.arg_reads src/game/unsorted/hot_batch_95.cpp
Exit 1 when a call passes fewer arguments than its callee reads.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence
from pathlib import Path

_NAMES = {"a0": 4, "a1": 5, "a2": 6, "a3": 7, "t0": 8, "t1": 9, "t2": 10, "t3": 11}
_NAME = {number: name for name, number in _NAMES.items()}
_LINE = re.compile(r"^\s*[0-9A-F]{8}: [0-9A-F]{8}\s+(\S+)\s*(.*)$")
_REGISTER = re.compile(r"\$(a[0-3]|t[0-3])\b")
# Instructions whose registers are all read: stores, branches, jumps, multiplies, moves to the FPU.
_READ_ALL = {"sw", "sh", "sb", "swl", "swr", "swc1", "beq", "bne", "beql", "bnel", "bgtz", "blez", "bltz", "bgez", "beqz", "bnez",
             "bgtzl", "blezl", "bltzl", "bgezl", "beqzl", "bnezl", "jr", "jalr", "mult", "multu", "div", "divu", "mtc1", "mthi", "mtlo"}
# Instructions that also read their destination (a conditional move keeps it, a left/right load merges into it).
_READ_DESTINATION = {"movz", "movn", "lwl", "lwr", "ins"}
_CALL = re.compile(r"call_(?:set|keep)\(rt, ctx, 0x([0-9A-Fa-f]{8})u, \{")


def arg_reads(lines: Sequence[str]) -> set[int]:
    """Register numbers (4-11) whose first mention in the disassembly, in address order, is a read."""
    first: dict[int, bool] = {}  # register -> first mention was a read
    for line in lines:
        match = _LINE.match(line)
        if not match:
            continue
        op, rest = match[1], match[2].split("  ")[0]
        registers = [_NAMES[name] for name in _REGISTER.findall(rest)]
        if not registers:
            continue
        operands = [piece.strip() for piece in rest.replace("(", ",").replace(")", "").split(",")]
        if op in _READ_ALL or not operands or not operands[0].startswith("$"):
            reads, writes = registers, []
        else:
            destination = _NAMES.get(operands[0].lstrip("$"))
            writes = [destination] if destination is not None else []
            reads = [_NAMES[name] for name in _REGISTER.findall(", ".join(operands[1:]))]
            if op in _READ_DESTINATION and destination is not None:
                reads.append(destination)
        for number in reads:
            first.setdefault(number, True)
        for number in writes:
            first.setdefault(number, False)
    return {number for number, was_read in first.items() if was_read}


def literal_calls(source: str) -> list[tuple[int, int, int]]:
    """(callee address, number of arguments passed, line number) for every call with a literal address."""
    found = []
    for match in _CALL.finditer(source):
        depth, pieces, filled, index = 1, 0, False, match.end()
        while index < len(source) and depth:
            char = source[index]
            if char in "{([":
                depth += 1
            elif char in "})]":
                depth -= 1
            elif char == "," and depth == 1:
                pieces += 1
            if depth >= 1 and char not in " \n\t,":
                filled = True
            index += 1
        found.append((int(match[1], 16), pieces + 1 if filled else 0, source.count("\n", 0, match.start()) + 1))
    return found


def missing(reads: set[int], passed: int) -> list[int]:
    """The read argument registers a call of `passed` arguments leaves unset (the first eight arguments are a0-a3, t0-t3)."""
    return sorted(number for number in reads if number >= 4 + passed)


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - reads the dump
    from tools.disasm import disassemble
    from tools.family_members import TextImage
    from tools.progress import LOAD_BASE, load_functions

    parser = argparse.ArgumentParser(prog="tools.arg_reads", description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path)
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions_tsv)
    cache: dict[int, set[int]] = {}
    found = 0
    for address, passed, line in literal_calls(args.source.read_text()):
        vaddr = address - LOAD_BASE
        if vaddr not in sizes:
            continue
        if address not in cache:
            words, relocs = image.function(vaddr, sizes[vaddr])
            cache[address] = arg_reads(disassemble(words, relocs, image.segments, address))
        gaps = missing(cache[address], passed)
        if gaps:
            print(f"{args.source}:{line}: call of func_{address:08X} passes {passed}, the callee reads {', '.join(_NAME[n] for n in gaps)}", file=sys.stderr)
            found += 1
    print(f"{found} calls pass fewer arguments than their callee reads")
    return 1 if found else 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
