"""Before/after table of the synthetic generator v2 (docs/superpowers/specs/2026-11-19-synthetic-pointer-graphs-design.md).

Reads a `GT_SYNTH_V2=0` log and a default (v2 on) log of the synthetic gate and says, per source file, how many installed functions
compared in v1 and how many more compare only thanks to v2 cases.

  .venv/bin/python -m tools.synth_v2 OFF.log ON.log [--src src/game/unsorted]
"""

from __future__ import annotations

import argparse
import re
from collections.abc import Mapping, Collection
from dataclasses import dataclass
from pathlib import Path

from tools.progress import registered_addresses
from tools.synth_reach import parse_synth_log

_V2 = re.compile(r"^\[decomp\]\s+v2 cases: compared (\d+), equal (\d+)", re.MULTILINE)
_FUNCTION = re.compile(r"@0x([0-9A-Fa-f]{8}): compared \d+")


@dataclass(frozen=True)
class Row:
    installed: int = 0
    v1: int = 0
    v2_new: int = 0
    scene: int = 0  # still not comparable, first fault at a tiny address: needs a scene


def parse_v2(text: str) -> dict[int, tuple[int, int]]:
    """address -> (v2 cases compared, equal), from the `v2 cases:` line after each function line."""
    out: dict[int, tuple[int, int]] = {}
    last: int | None = None
    for line in text.splitlines():
        if match := _FUNCTION.search(line):
            last = int(match.group(1), 16)
        elif last is not None and (v2 := _V2.match(line)):
            out[last] = (int(v2.group(1)), int(v2.group(2)))
            last = None
    return out


def newly_compared(off: str, on: str) -> set[int]:
    """Functions with no comparison in the v2-off run, and v2 cases that compared equal with no mismatch in the v2-on run."""
    before = parse_synth_log(off)
    after = parse_synth_log(on)
    cases = parse_v2(on)
    return {
        address
        for address, (compared, equal) in cases.items()
        if compared > 0 and compared == equal and before.get(address) is not None and before[address].compared == 0
        and address in after and after[address].mismatched == 0
    }


_FAULT = re.compile(r"first not comparable: original: faulted: Guest memory access outside PSP RAM/EDRAM at 0x([0-9A-Fa-f]+)")


def needs_scene(text: str) -> set[int]:
    """Functions whose first not-comparable reason is a fault at a tiny address (below 0x1000): a null game-state holder, which no generated
    input can fill (spec item 4): they need a scene route (GT_SYNTH_ROUTE), not another generator."""
    found: set[int] = set()
    last: int | None = None
    for line in text.splitlines():
        if match := _FUNCTION.search(line):
            last = int(match.group(1), 16)
        elif last is not None and (fault := _FAULT.search(line)):
            if int(fault.group(1), 16) < 0x1000:
                found.add(last)
            last = None
    return found


def table(off: str, on: str, files: Mapping[str, Collection[int]]) -> dict[str, Row]:
    before = parse_synth_log(off)
    new = newly_compared(off, on)
    scene = needs_scene(on) - new
    rows: dict[str, Row] = {}
    for name, addresses in files.items():
        installed = [a for a in addresses if a in before]
        rows[name] = Row(len(installed), sum(1 for a in installed if before[a].compared > 0), len(new & set(addresses)), len(scene & set(addresses)))
    return rows


def format_table(rows: Mapping[str, Row]) -> str:
    lines = [f"{'file':<20}{'installed':>10}{'v1 compared':>13}{'v2 newly':>10}{'needs scene':>13}"]
    for name, row in rows.items():
        lines.append(f"{name:<20}{row.installed:>10}{row.v1:>13}{row.v2_new:>10}{row.scene:>13}")
    lines.append(f"{'total':<20}{sum(r.installed for r in rows.values()):>10}{sum(r.v1 for r in rows.values()):>13}{sum(r.v2_new for r in rows.values()):>10}{sum(r.scene for r in rows.values()):>13}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("off", type=Path)
    parser.add_argument("on", type=Path)
    parser.add_argument("--src", type=Path, default=Path("src/game/unsorted"))
    args = parser.parse_args(argv)
    files = {p.stem: set(a for a in registered_addresses(p.read_text())) for p in sorted(args.src.glob("hot_batch_*.cpp"))}
    print(format_table({k: v for k, v in table(args.off.read_text(), args.on.read_text(), files).items() if v.installed}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
