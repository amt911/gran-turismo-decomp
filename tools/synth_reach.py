"""How many functions a synthetic gate run could compare (docs/superpowers/specs/2026-11-10-recorded-arguments-design.md).

Reads the per-function lines of a `gt_native --decomp diff` synthetic run (`GT_SYNTH=N`) and groups the functions by size:
how many have >= 1 equal case and no mismatch ("verified"), and how many of those also changed memory in some case.
Run it on two logs (`GT_SYNTH_CLASS=0` and the default) to see what class-matched arguments add.

  .venv/bin/python -m tools.synth_reach --log recomp/analysis/synth.log [--probe recomp/analysis/probe_*.log] \\
      [--functions recomp/analysis/functions.tsv]

`--probe` restricts the table to functions no route reached (hits == 0 in every probe log).
"""

from __future__ import annotations

import argparse
import re
from collections.abc import Collection, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

LOAD_BASE = 0x08804000
_LINE = re.compile(
    r"@0x([0-9A-Fa-f]{8}): compared (\d+), equal (\d+), mismatched (\d+), not comparable (\d+) "
    r"\(calls \d+, cases that changed memory (\d+)"
)
_PROBE = re.compile(r"^\[probe\] 0x([0-9A-Fa-f]{8}) hits=(\d+)", re.MULTILINE)
BUCKETS = ("<100", "100-300", "300-1000", ">=1000")


@dataclass(frozen=True)
class Result:
    compared: int
    equal: int
    mismatched: int
    skipped: int
    changed_memory: int

    @property
    def verified(self) -> bool:
        return self.equal > 0 and self.mismatched == 0


@dataclass(frozen=True)
class Row:
    functions: int = 0
    verified: int = 0
    verified_changed_memory: int = 0


def parse_synth_log(text: str) -> dict[int, Result]:
    out: dict[int, Result] = {}
    for m in _LINE.finditer(text):
        compared, equal, mismatched, skipped, changed = (int(g) for g in m.groups()[1:])
        out[int(m.group(1), 16)] = Result(compared, equal, mismatched, skipped, changed)
    return out


def reached_from_probe(text: str) -> set[int]:
    return {int(m.group(1), 16) for m in _PROBE.finditer(text) if int(m.group(2)) > 0}


def bucket(size: int) -> str:
    return BUCKETS[0] if size < 100 else BUCKETS[1] if size < 300 else BUCKETS[2] if size < 1000 else BUCKETS[3]


def summarize(results: Mapping[int, Result], sizes: Mapping[int, int], selected: Collection[int]) -> dict[str, Row]:
    rows = {name: Row() for name in BUCKETS}
    for address in selected:
        result = results.get(address)
        name = bucket(sizes.get(address, 0))
        row = rows[name]
        verified = result is not None and result.verified
        rows[name] = Row(row.functions + 1, row.verified + verified,
                         row.verified_changed_memory + (verified and result.changed_memory > 0))
    return rows


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--probe", nargs="*", default=[], type=Path)
    parser.add_argument("--functions", type=Path, default=Path("recomp/analysis/functions.tsv"))
    args = parser.parse_args(argv)
    results = parse_synth_log(args.log.read_text(errors="replace"))
    sizes = {}
    for line in args.functions.read_text().splitlines():
        if line.strip():
            entry, size = line.split("\t")
            sizes[int(entry, 16) + LOAD_BASE] = int(size)
    selected = set(results)
    if args.probe:
        reached: set[int] = set()
        for path in args.probe:
            reached |= reached_from_probe(path.read_text(errors="replace"))
        selected -= reached
    rows = summarize(results, sizes, selected)
    print(f"{'size':10} {'functions':>9} {'verified':>9} {'+changed memory':>16}")
    for name in BUCKETS:
        row = rows[name]
        print(f"{name:10} {row.functions:9} {row.verified:9} {row.verified_changed_memory:16}")
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
