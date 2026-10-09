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
_PPSSPP_CALL = re.compile(r"^[A-Z]\s+(?:\S+=)?([A-Za-z_]\w*)\(")
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


def game_calls(ppsspp_calls: Sequence[str], stub_names: dict[int, str]) -> list[str]:
    """Keep only PPSSPP log calls that are functions the game imports. PPSSPP also logs internal
    pseudo-calls (e.g. __KernelReturnFromMipsCall) that no game code ever makes."""
    imported = set(stub_names.values()) | {n.rstrip("0123456789") for n in stub_names.values()}
    return [c for c in ppsspp_calls if c in imported]


def _normalise(call: str, logged: set[str]) -> str:
    """PPSSPP names some import stubs with a suffix its call log omits
    ("Importing scePowerSetClockFrequency350" vs "scePowerSetClockFrequency(...)")."""
    if call in logged:
        return call
    stripped = call.rstrip("0123456789")
    return stripped if stripped in logged else call


def compare(ppsspp: Sequence[str], ours: Sequence[str], extra_names: set[str] | None = None,
            max_gap: int = 50) -> Comparison:
    """PPSSPP's calls must appear in our trace in the same order. Some calls PPSSPP logs only in
    some states (CheckCallback, ...), so we may have extra calls in between: up to max_gap
    consecutive extras are skipped. A call we skipped and PPSSPP needs later is a reordering and
    counts as a divergence, so a permutation can't hide behind the gap."""
    logged = set(ppsspp) | (extra_names or set())
    filtered = [n for n in (_normalise(c, logged) for c in ours) if n in logged]
    j = 0
    skipped: list[str] = []
    for i, call in enumerate(ppsspp):
        found = next((k for k in range(j, min(len(filtered), j + max_gap + 1)) if filtered[k] == call), None)
        if found is None:
            if j >= len(filtered) and call not in skipped:
                return Comparison(matched=i, divergence=None)  # our run simply stopped earlier
            got = filtered[j] if j < len(filtered) else "<end>"
            return Comparison(matched=i, divergence=Divergence(index=i, a_call=call, b_call=got,
                                                               context=list(ppsspp[max(0, i - 3):i])))
        skipped = filtered[j:found] if found > j else skipped
        j = found + 1
    return Comparison(matched=len(ppsspp), divergence=None)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("ppsspp_log", type=Path)
    parser.add_argument("gt_trace", type=Path)
    parser.add_argument("--limit", type=int, default=2000, help="calls that must match (default 2000)")
    args = parser.parse_args(argv)
    stubs = parse_ppsspp_imports(args.ppsspp_log)
    ours = parse_gt_trace(args.gt_trace, stubs)
    result = compare(game_calls(parse_ppsspp_log(args.ppsspp_log), stubs), ours)
    if result.divergence:
        d = result.divergence
        print(f"diverge at call {d.index}: ppsspp={d.a_call} gt={d.b_call} (after {', '.join(d.context)})")
    print(f"matched {result.matched} calls (gate: {args.limit})")
    return 0 if result.matched >= args.limit else 1


if __name__ == "__main__":
    sys.exit(main())
