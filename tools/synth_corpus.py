"""The synthetic-case corpus (docs/superpowers/specs/2026-11-11-coverage-guided-synthetic-inputs-design.md).

One recipe per line, as host/synth_case.cpp's `format_recipe` writes it: `ADDRESS INDEX SEED [reg:R=V] [word:OFFSET=V] [block:I=SEED]`
with V = imm:N | arena:OFFSET | live:K | class:K. A recipe holds no game bytes: values are small constants, arena-relative
offsets or indices. Files: config/synth_corpus/<ADDRESS>.txt, first line `# layout N` (the arena layout the recipes assume; a
file of another layout is skipped by the reader here and by the host).

  .venv/bin/python -m tools.synth_corpus merge config/synth_corpus recomp/analysis/gt_diff_TAG_dealer.log
"""

from __future__ import annotations

import argparse
import re
from collections.abc import Iterable, Sequence
from pathlib import Path

LAYOUT_VERSION = 1  # arena 64 KiB, 1 KiB blocks (host/synth_case.cpp)
DEFAULT_CAP = 16  # recipes kept per function

_NUMBER = r"(?:0[xX][0-9A-Fa-f]{1,8}|\d{1,10})"
_VALUE = rf"(?:imm:{_NUMBER}|arena:{_NUMBER}|live:{_NUMBER}|class:{_NUMBER})"
_MUTATION = rf"(?:reg:[0-7]={_VALUE}|word:{_NUMBER}={_VALUE}|block:{_NUMBER}={_NUMBER})"
_RECIPE = re.compile(rf"^[0-9A-F]{{8}} \d+ \d+(?: {_MUTATION})*$")
_CORPUS_LINE = re.compile(r"^\[corpus\] (.+)$", re.MULTILINE)


def _number(text: str) -> int:
    return int(text, 0)


def valid_recipe(line: str) -> bool:
    if not _RECIPE.match(line):
        return False
    for field in line.split()[3:]:
        kind, rest = field.split(":", 1)
        where, rhs = rest.split("=", 1)
        if _number(where) > 0xFFFFFFFF:
            return False
        if kind == "word" and _number(where) % 4:
            return False
        for number in re.findall(_NUMBER, rhs):
            if _number(number) > 0xFFFFFFFF:
                return False
        if rhs.startswith("arena:") and _number(rhs[6:]) % 4:
            return False
    return True


def address_of(line: str) -> int:
    return int(line.split()[0], 16)


def from_log(text: str) -> list[str]:
    return [m.group(1) for m in _CORPUS_LINE.finditer(text) if valid_recipe(m.group(1))]


def merge(old: Iterable[str], new: Iterable[str], cap: int = DEFAULT_CAP) -> list[str]:
    seen: set[str] = set()
    per: dict[int, int] = {}
    merged: list[str] = []
    for line in [*old, *new]:
        if line in seen:
            continue
        address = address_of(line)
        if per.get(address, 0) >= cap:
            continue
        seen.add(line)
        per[address] = per.get(address, 0) + 1
        merged.append(line)
    return merged


def write_corpus(directory: Path, lines: Iterable[str]) -> None:
    by_address: dict[int, list[str]] = {}
    for line in lines:
        by_address.setdefault(address_of(line), []).append(line)
    directory.mkdir(parents=True, exist_ok=True)
    for address, recipes in by_address.items():
        (directory / f"{address:08X}.txt").write_text(f"# layout {LAYOUT_VERSION}\n" + "\n".join(recipes) + "\n")


def read_corpus(directory: Path) -> list[str]:
    lines: list[str] = []
    if not directory.is_dir():
        return lines
    for path in sorted(directory.glob("*.txt")):
        rows = path.read_text().splitlines()
        if not rows or rows[0] != f"# layout {LAYOUT_VERSION}":
            continue
        lines.extend(row for row in rows[1:] if valid_recipe(row))
    return lines


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    merge_parser = sub.add_parser("merge", help="add the [corpus] lines of run logs to a corpus directory")
    merge_parser.add_argument("directory", type=Path)
    merge_parser.add_argument("logs", nargs="+", type=Path)
    merge_parser.add_argument("--cap", type=int, default=DEFAULT_CAP)
    args = parser.parse_args(argv)
    new: list[str] = []
    for log in args.logs:
        new.extend(from_log(log.read_text(errors="replace")))
    write_corpus(args.directory, merge(read_corpus(args.directory), new, args.cap))
    print(f"{len(new)} recipes read from {len(args.logs)} log(s); corpus now {len(read_corpus(args.directory))}")
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
