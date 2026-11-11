"""Batches of the synthetic-corpus search (docs/superpowers/specs/2026-11-11-coverage-guided-synthetic-inputs-design.md).

`plan` lists the decompiled functions no gate log verified and groups them by source file into batches (a batch is a set of whole
files, so the coverage build instruments few files); `summary` compares a corpus-off and a corpus-on synthetic log (verified before /
after, mismatches) and `pick` names a newly verified function for the sabotage pair. `tools/corpus_batch.sh N` runs one batch end to end.

  .venv/bin/python -m tools.corpus_batch plan --out recomp/analysis/batches LOG...    (logs: gt_diff_*.log of a verdict)
  .venv/bin/python -m tools.corpus_batch summary B.log A.log
"""

from __future__ import annotations

import argparse
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from tools.progress import registered_addresses
from tools.synth_reach import parse_synth_log


def unverified(logs: Iterable[str]) -> list[int]:
    """Functions in the logs with no equal case anywhere, or a mismatch anywhere."""
    best: dict[int, tuple[int, int]] = {}
    for text in logs:
        for address, result in parse_synth_log(text).items():
            equal, mismatched = best.get(address, (0, 0))
            best[address] = (max(equal, result.equal), mismatched + result.mismatched)
    return sorted(a for a, (equal, mismatched) in best.items() if equal == 0 or mismatched > 0)


def group_by_file(addresses: Iterable[int], registrations: Mapping[str, Iterable[int]]) -> dict[str, list[int]]:
    wanted = set(addresses)
    grouped = {f: sorted(a for a in found if a in wanted) for f, found in registrations.items()}
    return {f: found for f, found in grouped.items() if found}


def make_batches(by_file: Mapping[str, Sequence[int]], must: Sequence[str] = (), max_functions: int = 160,
                 max_files: int = 30) -> list[list[str]]:
    """Whole files, largest first after `must`, closing a batch at `max_functions` functions or `max_files` files."""
    order = [*must, *sorted((f for f in by_file if f not in must), key=lambda f: -len(by_file[f]))]
    batches: list[list[str]] = []
    current: list[str] = []
    count = 0
    for name in order:
        size = len(by_file[name])
        if current and count + size > max_functions:
            batches.append(current)
            current, count = [], 0
        current.append(name)
        count += size
        if len(current) >= max_files:
            batches.append(current)
            current, count = [], 0
    if current:
        batches.append(current)
    return batches


@dataclass(frozen=True)
class Summary:
    functions: int
    before: int
    after: int
    mismatched: list[int]


def summary(before: str, after: str) -> Summary:
    a, b = parse_synth_log(before), parse_synth_log(after)
    return Summary(len(b), sum(r.verified for r in a.values()), sum(r.verified for r in b.values()),
                   sorted(x for x, r in b.items() if r.mismatched))


def pick_sabotage(before: str, after: str, registry: Mapping[int, str]) -> tuple[int, str] | None:
    """A function verified only with the corpus whose cases changed memory, and its source file."""
    a, b = parse_synth_log(before), parse_synth_log(after)
    for address, result in sorted(b.items()):
        was = a.get(address)
        if result.verified and result.changed_memory > 0 and not (was and was.verified) and address in registry:
            return address, registry[address]
    return None


def _registry(src: Path) -> dict[str, list[int]]:
    return {str(p): registered_addresses(p.read_text()) for p in sorted(src.rglob("*.cpp"))}


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    plan = sub.add_parser("plan")
    plan.add_argument("logs", nargs="+", type=Path)
    plan.add_argument("--src", type=Path, default=Path("src"))
    plan.add_argument("--out", type=Path, required=True)
    plan.add_argument("--first", type=int, default=1, help="number of the first batch file")
    plan.add_argument("--must", nargs="*", default=[], help="source files that open the first batch")
    plan.add_argument("--max-functions", type=int, default=160)
    summ = sub.add_parser("summary")
    summ.add_argument("before", type=Path)
    summ.add_argument("after", type=Path)
    pick = sub.add_parser("pick")
    pick.add_argument("before", type=Path)
    pick.add_argument("after", type=Path)
    pick.add_argument("--src", type=Path, default=Path("src"))
    args = parser.parse_args(argv)
    if args.command == "plan":
        todo = unverified(p.read_text(errors="replace") for p in args.logs)
        by_file = group_by_file(todo, _registry(args.src))
        batches = make_batches(by_file, args.must, args.max_functions)
        args.out.mkdir(parents=True, exist_ok=True)
        for number, files in enumerate(batches, args.first):
            functions = sorted(a for f in files for a in by_file[f])
            (args.out / f"batch{number}.txt").write_text(",".join(f"0x{a:08X}" for a in functions))
        print(f"{len(todo)} unverified functions in {len(by_file)} files: {len(batches)} batches")
        return 0
    before, after = args.before.read_text(errors="replace"), args.after.read_text(errors="replace")
    if args.command == "summary":
        s = summary(before, after)
        print(f"functions {s.functions} before {s.before} after {s.after} mismatch {[hex(a) for a in s.mismatched]}")
        return 0
    chosen = pick_sabotage(before, after, {a: f for f, found in _registry(args.src).items() for a in found})
    print(f"0x{chosen[0]:08X} {chosen[1]}" if chosen else "none")
    return 0 if chosen else 1


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
