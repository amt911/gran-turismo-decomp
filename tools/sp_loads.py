"""Loads of callee-saved registers from the stack that are no restore: the ones a hidden epilogue would also hide.

Reading a function's disassembly is easier with its prologue and epilogue (`sw`/`lw` of $s0-$s7, $fp, $ra to the frame)
filtered out, and that filter also drops a REAL `lw $s0, 4($sp)` (batch 94: func_08A1F328 reloaded $s0 from the word
a callee had stored at sp + 4; the decompiled version kept the old object and faulted on every case). This lists, per function
of a batch file, the `lw $sN/$fp/$ra, off($sp)` whose register was never saved to that offset: those are values to read.

Usage (from the repository root): .venv/bin/python -m tools.sp_loads src/game/unsorted/hot_batch_94.cpp
Exit 1 when one is found (they need a look, a clean file has none or only the ones the C++ already models).
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence
from pathlib import Path

from tools.literal_check import function_literals

_SAVE = re.compile(r"\bsw\s+\$(s\d|fp|ra), (0x[0-9A-Fa-f]+)\(\$sp\)")
_LOAD = re.compile(r"\blw\s+\$(s\d|fp|ra), (0x[0-9A-Fa-f]+)\(\$sp\)")


def real_loads(lines: Sequence[str]) -> list[str]:
    """The lines of a function's disassembly that load a callee-saved register from sp at an offset it was not saved to."""
    saved = {(m[1], int(m[2], 16)) for line in lines if (m := _SAVE.search(line))}
    return [line.strip() for line in lines if (m := _LOAD.search(line)) and (m[1], int(m[2], 16)) not in saved]


def functions(source: str) -> list[int]:
    """The runtime addresses of the `void func_X(` definitions of a batch file."""
    return sorted(function_literals(source))


def main(argv: Sequence[str] | None = None) -> int:  # pragma: no cover - reads the dump
    from tools.disasm import disassemble
    from tools.family_members import TextImage
    from tools.progress import LOAD_BASE, load_functions

    parser = argparse.ArgumentParser(prog="tools.sp_loads", description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path)
    parser.add_argument("--elf", type=Path, default=Path("extracted/EBOOT.elf"))
    parser.add_argument("--functions-tsv", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    image = TextImage.from_elf(args.elf.read_bytes())
    sizes = load_functions(args.functions_tsv)
    found = 0
    for address in functions(args.source.read_text()):
        vaddr = address - LOAD_BASE
        words, relocs = image.function(vaddr, sizes[vaddr])
        for line in real_loads(disassemble(words, relocs, image.segments, address)):
            print(f"func_{address:08X}: {line}", file=sys.stderr)
            found += 1
    print(f"{found} loads of callee-saved registers from the stack at an offset they were not saved to")
    return 1 if found else 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
