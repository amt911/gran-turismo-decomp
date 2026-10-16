"""Every hexadecimal literal of a decompiled function must appear in that function's own disassembly.

A constant typed into a batch file on a path the routes never run (batch 90: 0x11111111 for the asm's 0x01111111, built by
`lui 0x111` / `ori 0x1111`) is invisible to the differential gate. This tool compares, per `void func_XXXXXXXX(...)` of a
source file, the hexadecimal literals of its body (comments stripped) with the values `tools.disasm` prints for the function:
immediates and displacements, resolved `lui`/`addiu` addresses, call targets and the 32-bit values of a `lui`/`ori` pair or a
negative immediate. A literal below 5 is skipped (loop bounds, small offsets). What it cannot check is reported, not hidden: a
literal the asm builds in two steps that are not an address (a `lui` / `ori` pair) must be listed by hand with `--allow`.

Usage (from the repository root): .venv/bin/python -m tools.literal_check src/game/unsorted/hot_batch_91.cpp [--allow 0xADDR:0xVALUE ...]
Exit 1 when a literal is missing.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path

_FUNCTION = re.compile(r"^(?:\[\[maybe_unused\]\] )?void func_([0-9A-F]{8})\(", re.MULTILINE)
_LITERAL = re.compile(r"\b0x([0-9A-Fa-f]+)u?\b")


def strip_comments(text: str) -> str:
    return "\n".join(line.split("//")[0] for line in text.splitlines())


def function_literals(source: str) -> dict[int, set[int]]:
    """Runtime address of each `void func_X(` -> the hexadecimal literals (>= 5) of its body."""
    found: dict[int, set[int]] = {}
    starts = [(m.start(), int(m[1], 16)) for m in _FUNCTION.finditer(source)]
    for index, (start, address) in enumerate(starts):
        line_end = source.find("\n", start)
        first_line = source[start:line_end if line_end >= 0 else len(source)]
        if first_line.rstrip().endswith("}"):  # a one-line wrapper
            body = first_line
        else:
            end = source.find("\n}\n", start)
            body = source[start:end if end >= 0 else len(source)]
        found[address] = {value for text in _LITERAL.findall(strip_comments(body)) if (value := int(text, 16)) >= 5}
    return found


def disassembly_values(lines: Sequence[str]) -> set[int]:
    """Every value the disassembly prints: hexadecimal operands, resolved addresses, call targets, 32-bit negatives."""
    values: set[int] = set()
    text = "\n".join(lines)
    for match in re.finditer(r"(-?)0x([0-9A-Fa-f]+)", text):
        value = int(match[2], 16)
        values.add(value)
        if match[1]:
            values.add(-value & 0xFFFFFFFF)
    for match in re.finditer(r"func_([0-9A-F]{8})", text):
        values.add(int(match[1], 16))
    for match in re.finditer(r"\$zero, (-?\d+)", text):
        values.add(int(match[1]) & 0xFFFFFFFF)
    return values


def pair_values(lines: Sequence[str]) -> set[int]:
    """The 32-bit values built by `lui rX, H` followed by `ori`/`addiu rX, rX, L` on the same register."""
    values: set[int] = set()
    high: dict[str, int] = {}
    for line in lines:
        fields = line.split(None, 3)
        if len(fields) < 3:
            continue
        op = fields[2]
        rest = re.split(r"\[|  ->| = ", line.split(op, 1)[1])[0] if op in line else ""
        operands = rest.replace(" ", "").split(",")
        if op == "lui" and len(operands) == 2 and operands[1].startswith("0x"):
            high[operands[0]] = int(operands[1], 16) << 16
            values.add(high[operands[0]])  # a float constant is a bare lui
        elif op in ("ori", "addiu") and len(operands) == 3 and operands[1] in high and operands[0] == operands[1]:
            low = int(operands[2], 16) if not operands[2].startswith("-") else -int(operands[2][1:], 16)
            values.add((high[operands[1]] + low) & 0xFFFFFFFF)
    return values


def check(source: str, disassemble: Callable[[int], list[str]], allow: Mapping[int, set[int]] | None = None) -> list[tuple[int, int]]:
    """(function, literal) pairs whose literal is in no value of the function's disassembly."""
    missing = []
    for address, literals in sorted(function_literals(source).items()):
        lines = disassemble(address)
        known = disassembly_values(lines) | pair_values(lines) | (allow or {}).get(address, set())
        missing.extend((address, value) for value in sorted(literals - known))
    return missing


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - reads the dump
    from tools.disasm import disassemble
    from tools.family_members import TextImage
    from tools.progress import LOAD_BASE, load_functions

    parser = argparse.ArgumentParser(prog="tools.literal_check", description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path)
    parser.add_argument("--allow", action="append", default=[], metavar="0xFUNC:0xVALUE")
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions_tsv)
    allow: dict[int, set[int]] = {}
    for item in args.allow:
        function, _, value = item.partition(":")
        allow.setdefault(int(function, 16), set()).add(int(value, 16))

    def lines_of(address: int) -> list[str]:
        vaddr = address - LOAD_BASE
        words, relocs = image.function(vaddr, sizes[vaddr])
        return disassemble(words, relocs, image.segments, address)

    source = args.source.read_text()
    missing = check(source, lines_of, allow)
    for address, value in missing:
        print(f"func_{address:08X}: literal 0x{value:X} is in no value of its disassembly", file=sys.stderr)
    print(f"{len(function_literals(source))} functions, {len(missing)} literals missing")
    return 1 if missing else 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
