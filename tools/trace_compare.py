"""Compare GT's import-call trace (gt_native) with the HLE calls PPSSPP logs for the original.

Usage: .venv/bin/python -m tools.trace_compare PPSSPP_LOG GT_TRACE [--limit N]

gt_native records the import *stub address* of every call; PPSSPP's own "Importing NAME : STUB"
lines (same binary) turn those addresses into names, so no external NID database is needed.
PPSSPP only logs the HLE calls its implementations log, so our trace is first restricted to the
function names that appear in PPSSPP's log at all; then both sequences must agree call by call.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

# "I 276=sceKernelCreateThread(...)", "D 00000112=sceKernelGetModuleIdByAddress(...): 112",
# "D sceIoOpen(...): 0000000a". Non-call lines ("D Context switch: ...") don't match.
_PPSSPP_CALL = re.compile(r"^[A-Z]\s+(?:[0-9A-Fa-f]+=)?([A-Za-z_]\w*)\(")
# "D Importing sceKernelUnloadModule : 08c7a8c8" (not "D Importing Module X, stubs at ...").
_PPSSPP_IMPORT = re.compile(r"^[A-Z]\s+Importing\s+(\w+)\s*:\s*([0-9A-Fa-f]{8})\b")


@dataclass(frozen=True)
class Divergence:
    index: int
    a_call: str
    b_call: str
    context: list[str]


@dataclass(frozen=True)
class Comparison:
    matched: int
    divergence: Divergence | None


def parse_ppsspp_log(path: Path) -> list[str]:
    calls = []
    with path.open(errors="replace") as f:
        for line in f:
            m = _PPSSPP_CALL.match(line)
            if m:
                calls.append(m.group(1))
    return calls


def parse_ppsspp_imports(path: Path) -> dict[int, str]:
    stubs: dict[int, str] = {}
    with path.open(errors="replace") as f:
        for line in f:
            m = _PPSSPP_IMPORT.match(line)
            if m:
                stubs[int(m.group(2), 16)] = m.group(1)
    return stubs


def parse_gt_trace(path: Path, stub_names: dict[int, str] | None = None) -> list[str]:
    """Lines are '<index> 0x<stub>'; stubs PPSSPP named become names, others stay as written."""
    names = stub_names or {}
    calls = []
    for line in path.read_text().splitlines():
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        token = parts[1].strip()
        try:
            calls.append(names.get(int(token, 16), token))
        except ValueError:
            calls.append(token)
    return calls


def first_divergence(a: Sequence[str], b: Sequence[str]) -> Divergence | None:
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return Divergence(index=i, a_call=x, b_call=y, context=list(a[max(0, i - 3):i]))
    return None


def compare(ppsspp: Sequence[str], ours: Sequence[str]) -> Comparison:
    logged = set(ppsspp)
    filtered = [c for c in ours if c in logged]
    div = first_divergence(ppsspp, filtered)
    matched = div.index if div else min(len(ppsspp), len(filtered))
    return Comparison(matched=matched, divergence=div)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("ppsspp_log", type=Path)
    parser.add_argument("gt_trace", type=Path)
    parser.add_argument("--limit", type=int, default=2000, help="calls that must match (default 2000)")
    args = parser.parse_args(argv)
    ours = parse_gt_trace(args.gt_trace, parse_ppsspp_imports(args.ppsspp_log))
    result = compare(parse_ppsspp_log(args.ppsspp_log), ours)
    if result.divergence:
        d = result.divergence
        print(f"diverge at call {d.index}: ppsspp={d.a_call} gt={d.b_call} (after {', '.join(d.context)})")
    print(f"matched {result.matched} calls (gate: {args.limit})")
    return 0 if result.matched >= args.limit else 1


if __name__ == "__main__":
    sys.exit(main())
