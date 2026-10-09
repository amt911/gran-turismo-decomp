"""Turn the Adhoc native-method map into config/symbol_addrs.txt entries.

Usage: .venv/bin/python -m tools.adhoc_symbols recomp/analysis/adhoc_natives.tsv > config/symbol_addrs.txt

Input: the lines tools/ghidra_scripts/AdhocNatives.java writes — registrar, initializer, the
identifier strings that initializer uses, method name, function (ELF vaddrs, hex). GT registers each
native method an Adhoc script can call with its name (FUN_00382fb0 for module functions, FUN_00383108
for class methods), so the name is the one the game's scripts use (docs/FACTS.md, Adhoc bridge).

The module is the initializer's first `M<Upper>...` class string (MLocale, MSystemBGM...); when there
is none the initializer address stands in. One name per address: the first registration wins; two
functions under one name in one module get their address appended.
"""

from __future__ import annotations

import re
import sys
from collections.abc import Sequence
from pathlib import Path

LOAD_BASE = 0x08804000
_CLASS = re.compile(r"^M[A-Z][A-Za-z0-9_]*$")

Native = tuple[int, int, list[str], str, int]  # registrar, initializer, strings, method, function


def load_natives(path: Path) -> list[Native]:
    natives: list[Native] = []
    for line in path.read_text().splitlines():
        fields = line.split("\t")
        if len(fields) != 6 or fields[0] != "NATIVE":
            continue
        strings = [s for s in fields[3].split(",") if s]
        natives.append((int(fields[1], 16), int(fields[2], 16), strings, fields[4], int(fields[5], 16)))
    return natives


def module_name(strings: Sequence[str], initializer: int) -> str:
    for s in strings:
        if _CLASS.match(s):
            return s
    return f"adhoc_init_{initializer:06X}"


def symbol_lines(natives: Sequence[Native]) -> list[str]:
    named: dict[int, str] = {}
    evidence: dict[int, str] = {}
    by_name: dict[str, set[int]] = {}
    for registrar, initializer, strings, method, function in natives:
        by_name.setdefault(f"{module_name(strings, initializer)}::{method}", set()).add(function)
    for registrar, initializer, strings, method, function in natives:
        if function in named:
            continue
        name = f"{module_name(strings, initializer)}::{method}"
        if len(by_name[name]) > 1:
            name = f"{name}_{function + LOAD_BASE:08X}"
        named[function] = name
        evidence[function] = f"registrar {registrar:#x}, initializer {initializer:#x}"
    lines = []
    for function in sorted(named):
        lines.append(f"{named[function]} = 0x{function + LOAD_BASE:08X}; // adhoc native ({evidence[function]})")
    return lines


def main(argv: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if len(args) != 1:
        print(__doc__.splitlines()[2], file=sys.stderr)
        return 2
    print("// GT symbol names, runtime addresses (ELF vaddr + 0x08804000). One name per address.")
    print("// Generated from Adhoc native registrations: .venv/bin/python -m tools.adhoc_symbols")
    for line in symbol_lines(load_natives(Path(args[0]))):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
