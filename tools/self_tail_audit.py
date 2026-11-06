"""Decompiled functions that tail-call their own entry (`tail_call`, `jump` or `tail_with` to their own address).

A decompiled function ends with ctx.pc == its entry when it returns normally; a tail call sets ctx.pc to the target.
A tail call to the function's own entry is therefore indistinguishable from a return: the trampoline, the fibers and
call_here all take it as one, and the recursion never runs. The original's `j` to itself has to be written as a loop.

  .venv/bin/python -m tools.self_tail_audit [FILE.cpp ...]     (default: every .cpp under src/)
"""

from __future__ import annotations

import re
import sys
from collections.abc import Iterable
from pathlib import Path

_FUNCTION = re.compile(r"^void func_([0-9A-Fa-f]{8})\(psprecomp::Runtime[^)]*\)\s*\{", re.MULTILINE)
_TAIL = r"\b(?:tail_call|jump|tail_with)\(ctx,\s*0x{address}u?\b"


def self_tail_calls(text: str) -> list[str]:
    """Addresses (as written) of the functions in `text` whose body tail-calls their own entry."""
    starts = list(_FUNCTION.finditer(text))
    found = []
    for index, match in enumerate(starts):
        end = starts[index + 1].start() if index + 1 < len(starts) else len(text)
        body = text[match.end():end]
        if re.search(_TAIL.format(address=match.group(1)), body, re.IGNORECASE):
            found.append(match.group(1))
    return found


def audit(paths: Iterable[Path]) -> list[str]:
    """`path: address` for every self tail call in `paths`."""
    return [f"{path}: {address}" for path in paths for address in self_tail_calls(path.read_text(errors="replace"))]


def main(argv: list[str] | None = None) -> int:  # pragma: no cover - CLI wiring
    args = argv if argv is not None else sys.argv[1:]
    paths = [Path(a) for a in args] or sorted(Path("src").rglob("*.cpp"))
    found = audit(paths)
    for line in found:
        print(line)
    print(f"self tail audit: {len(paths)} files, {len(found)} self tail calls")
    return 1 if found else 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
