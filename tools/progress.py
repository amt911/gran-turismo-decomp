"""Decompilation progress: decompiled functions / all functions, and the bytes of .text they cover.

Usage: .venv/bin/python -m tools.progress [--functions recomp/analysis/functions.tsv] [--src src]

The function list comes from Ghidra (tools/ghidra_scripts/ExportFunctions.java, "entry<TAB>size" per
line, ELF vaddrs in hex). Decompiled functions are the GT_DECOMP(...) registrations under src/; a
function counts as SDK/library code when its file lives under one of SDK_DIRS, otherwise as game code
(docs/agents/decomp-workflow.md: the two numbers are reported separately, never merged).
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path

LOAD_BASE = 0x08804000  # runtime address of ELF vaddr 0 (docs/FACTS.md)
SDK_DIRS = ("libc", "libm", "libstdcxx", "libgcc", "sdk")
# Only active registrations (GT_DECOMP, or a GT_DECOMP_<KIND> template-instance registration):
# one behind `//` is decompiled but not swapped in (not comparable yet).
_REGISTRATION = re.compile(r"^[ \t]*GT_DECOMP(?:_[A-Z_]+)?\(\s*0x([0-9A-Fa-f]+)u?\s*[,)]", re.MULTILINE)


def load_functions(path: Path) -> dict[int, int]:
    functions: dict[int, int] = {}
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        entry, size = line.split("\t")
        functions[int(entry, 16)] = int(size)
    return functions


def decompiled_functions(src: Path) -> dict[int, str]:
    """ELF vaddr -> "sdk" or "game" for every GT_DECOMP registration under `src`."""
    found: dict[int, str] = {}
    for path in sorted(src.rglob("*.cpp")):
        kind = "sdk" if path.relative_to(src).parts[0] in SDK_DIRS else "game"
        for match in _REGISTRATION.finditer(path.read_text()):
            runtime = int(match.group(1), 16)
            if runtime < LOAD_BASE:
                raise ValueError(f"{path}: GT_DECOMP address {runtime:#010x} is below the load base")
            found[runtime - LOAD_BASE] = kind
    return found


def report(functions: Mapping[int, int], decompiled: Mapping[int, str]) -> list[str]:
    total_bytes = sum(functions.values())
    done = {entry: kind for entry, kind in decompiled.items() if entry in functions}
    done_bytes = sum(functions[entry] for entry in done)
    lines = [
        f"functions: {len(done)} / {len(functions)} decompiled ({100.0 * len(done) / max(1, len(functions)):.3f} %)",
        f"bytes: {done_bytes} / {total_bytes} ({100.0 * done_bytes / max(1, total_bytes):.3f} %)",
    ]
    for kind in ("game", "sdk"):
        entries = [entry for entry, k in done.items() if k == kind]
        lines.append(f"  {kind}: {len(entries)} functions, {sum(functions[e] for e in entries)} bytes")
    for entry in sorted(set(decompiled) - set(functions)):
        lines.append(f"warning: {entry + LOAD_BASE:#010x} is not a known function entry")
    return lines


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    parser.add_argument("--src", type=Path, default=Path("src"))
    args = parser.parse_args(argv)
    if not args.functions.is_file():
        print(f"{args.functions} not found: export it with tools/ghidra_scripts/ExportFunctions.java "
              "(docs/COMPILAR.md §8)", file=sys.stderr)
        return 1
    for line in report(load_functions(args.functions), decompiled_functions(args.src)):
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
