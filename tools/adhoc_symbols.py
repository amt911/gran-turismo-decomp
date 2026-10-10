"""Turn the Adhoc native-method map into config/symbol_addrs.txt entries.

Usage: .venv/bin/python -m tools.adhoc_symbols recomp/analysis/adhoc_natives.tsv [--keep config/symbol_addrs.txt] \
           > new_symbol_addrs.txt

--keep carries over the hand-written names of an existing file (every line that isn't an
"adhoc native" one); they own their addresses.

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
# Initializers whose class name isn't an M* string, named from what they register.
KNOWN_MODULES = {
    0x276614: "Object",  # isInstanceOf, getDeepCopy, toBool..toDouble, toString, defined: the root class
    0x29F0EC: "Math",    # strings "Math", "PI"; abs, sin, cos, ..., clamp, lerp, sqrt
    0x27AF54: "Module",  # string "Module"; load, defineModule, defineClass, import, clearStatic
}

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
    if initializer in KNOWN_MODULES:
        return KNOWN_MODULES[initializer]
    for s in strings:
        if _CLASS.match(s):
            return s
    return f"adhoc_init_{initializer:06X}"


def hand_lines(path: Path) -> dict[int, str]:
    """Runtime address -> line, for the hand-written names of an existing symbol file."""
    kept: dict[int, str] = {}
    for line in path.read_text().splitlines():
        match = re.match(r"^\S+ = 0x([0-9A-Fa-f]{8});", line)
        if match and "// adhoc native" not in line:
            kept[int(match.group(1), 16)] = line
    return kept


def symbol_lines(natives: Sequence[Native], kept: dict[int, str] | None = None) -> list[str]:
    kept = kept or {}
    named: dict[int, str] = {}
    evidence: dict[int, str] = {}
    by_name: dict[str, set[int]] = {}
    for registrar, initializer, strings, method, function in natives:
        by_name.setdefault(f"{module_name(strings, initializer)}::{method}", set()).add(function)
    for registrar, initializer, strings, method, function in natives:
        if function in named or function + LOAD_BASE in kept:
            continue
        name = f"{module_name(strings, initializer)}::{method}"
        if len(by_name[name]) > 1:
            name = f"{name}_{function + LOAD_BASE:08X}"
        named[function] = name
        evidence[function] = f"registrar {registrar:#x}, initializer {initializer:#x}"
    entries = {function + LOAD_BASE: f"{named[function]} = 0x{function + LOAD_BASE:08X}; "
               f"// adhoc native ({evidence[function]})" for function in named}
    entries.update(kept)
    return [entries[address] for address in sorted(entries)]


def main(argv: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    kept: dict[int, str] = {}
    if len(args) == 3 and args[1] == "--keep":
        kept = hand_lines(Path(args[2]))
        args = args[:1]
    if len(args) != 1:
        print(__doc__.splitlines()[2], file=sys.stderr)
        return 2
    print("// GT symbol names, runtime addresses (ELF vaddr + 0x08804000). One name per address.")
    print("// Adhoc natives are generated (.venv/bin/python -m tools.adhoc_symbols ... --keep this file);")
    print("// every other line is hand-written with its evidence and survives regeneration.")
    for line in symbol_lines(load_natives(Path(args[0])), kept):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
